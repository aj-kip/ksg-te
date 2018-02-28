/****************************************************************************

    File: TextLines.cpp
    Author: Andrew Janke
    License: GPLv3

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#include "TextLines.hpp"
#include "LuaCodeModeler.hpp"

#include <limits>
#include <stdexcept>
#include <iostream>
#include <functional>

#include <cassert>

namespace {

void do_text_lines_unit_tests();

} // end of <anonymous> namespace

TextLines::TextLines():
    m_rendering_options(&RenderOptions::get_default_instance()),
    m_width_constraint(std::numeric_limits<int>::max())
{}

/* explicit */ TextLines::TextLines(const std::u32string & content_):
    m_rendering_options(&RenderOptions::get_default_instance())
{ set_content(content_); }


void TextLines::constrain_to_width(int target_width) {
    m_width_constraint = target_width;
    for (auto & line : m_lines) {
        line.constrain_to_width(target_width);
    }
}

void TextLines::set_content(const std::u32string & content_string) {
    std::size_t index = 0;
    auto index_to_iterator = [&content_string](std::size_t index) {
        if (index == std::u32string::npos)
            return content_string.end();
        else
            return content_string.begin() + int(index);
    };
    while (true) {
        auto next = content_string.find(NEW_LINE, index);
        auto beg = index_to_iterator(index);
        auto end = index_to_iterator(next );
        m_lines.emplace_back(std::u32string(beg, end));
        m_lines.back().assign_ranges_updater(*this);
        m_lines.back().set_line_number(int(m_lines.size() - 1));
        if (end == content_string.end()) break;
        assert(next + 1 < content_string.size());
        index = next + 1;
    }
    check_invarients();
}

void TextLines::assign_render_options(const RenderOptions & options) {
    m_rendering_options = &options;
    for (auto & line : m_lines)
        line.assign_render_options(options);
}

void TextLines::assign_default_render_options() {
    assign_render_options(RenderOptions::get_default_instance());
}

Cursor TextLines::push(Cursor cursor, UChar uchar) {
    verify_cursor_validity("TextLines::push", cursor);
    if (cursor == end_cursor()) {
        m_lines.emplace_back();
        // need to tell the new line a few things
        auto & new_line = m_lines.back();
        new_line.constrain_to_width(m_width_constraint);
        new_line.assign_render_options(*m_rendering_options);
        new_line.assign_ranges_updater(*this);
        new_line.set_line_number(int(m_lines.size()) - 1);
    }
    auto & line = m_lines[std::size_t(cursor.line)];
    auto resp = line.push(cursor.column, uchar);
    if (resp == TextLine::SPLIT_REQUESTED) {
        // if split is requested, the line has not been modified
        auto spl = line.split(cursor.column);
        spl.assign_ranges_updater(*this);
        spl.assign_render_options(*m_rendering_options);
        spl.constrain_to_width(m_width_constraint);

        m_lines.insert(m_lines.begin() + cursor.line + 1, spl);
        {
        int line_num = 0;
        for (auto & line : m_lines)
            line.set_line_number(line_num++);
        }
        ++cursor.line;
        cursor.column = 0;
        LuaCodeModeler lcm;
        update_ranges_impl(lcm);
        check_invarients();
        return cursor;
    }
    // we know resp is a new column position now
    auto new_col = resp;
    check_invarients();
    return Cursor(cursor.line, new_col);
}

Cursor TextLines::delete_ahead(Cursor cursor) {
    verify_cursor_validity("TextLines::delete_ahead", cursor);
    if (cursor == end_cursor()) return cursor;
    auto & line = m_lines[std::size_t(cursor.line)];
    auto resp = line.delete_ahead(cursor.column);
    if (resp == TextLine::MERGE_REQUESTED) {
        // merge with the next line if it exists
        if (cursor.line + 1 >= int(m_lines.size()))
            return cursor;
        auto & next_line = m_lines[std::size_t(cursor.line + 1)];
        auto old_line_size = next_line.content_length();
        line.take_contents_of(next_line, TextLine::PLACE_AT_END);
        // note: line, next_line become danglers after this statement
        //       so it's important that we do not access them again
        m_lines.erase(m_lines.begin() + cursor.line + 1);
        if (cursor.line >= int(m_lines.size())) {
            int line_num = cursor.line;
            auto itr = m_lines.begin() + line_num;
            for (; itr != m_lines.end(); ++itr)
                itr->set_line_number(line_num++);
        }
        check_invarients();
        return Cursor(cursor.line, old_line_size);
    } else {
        auto new_col = resp;
        return Cursor(cursor.line, new_col);
    }
}

Cursor TextLines::delete_behind(Cursor cursor) {
    // unfinished
    verify_cursor_validity("TextLines::delete_behind", cursor);
    if (cursor == end_cursor()) {
        if (m_lines.empty()) return cursor;
        return Cursor(int(m_lines.size()) - 1, m_lines.back().content_length());
    } else if (cursor == Cursor()) {
        return cursor;
    }
    auto & line = m_lines[std::size_t(cursor.line)];
    auto resp = line.delete_behind(cursor.column);
    if (resp == TextLine::MERGE_REQUESTED) {
        assert(cursor.line > 0);
        auto & prev_line = m_lines[std::size_t(cursor.line - 1)];
        prev_line.take_contents_of(line, TextLine::PLACE_AT_END);
        auto line_size = line.content_length();
        // invalidates: prev_line, line
        m_lines.erase(m_lines.begin() + cursor.line);
        check_invarients();
        return Cursor(cursor.line - 1, line_size);
    }
    return Cursor(cursor.line, cursor.column - 1);
}

Cursor TextLines::wipe(Cursor beg, Cursor end) {
    verify_cursor_validity("TextLines::wipe (for beg)", beg);
    verify_cursor_validity("TextLines::wipe (for end)", end);
    auto wipe_chars = [this](int line_idx, int line_beg, int line_end) {
        auto & line = m_lines[std::size_t(line_idx)];
        line.wipe(line_beg, line_end);
    };
    for_each_line_in_range(beg, end, wipe_chars);
    // merge two end lines
    if (beg.line != end.line && end.line != int(m_lines.size())) {
        m_lines[std::size_t(beg.line)].take_contents_of
            (m_lines[std::size_t(end.line)], TextLine::PLACE_AT_END);
    }
    // delete inbetweens [beg.line + 1 end]
    //                or [beg.line + 1 end)

    assert(beg.line + 1 <= int(m_lines.size()));
    if (end.line != int(m_lines.size()))
        ++end.line;
    assert(beg.line + 1 <= end.line);
    m_lines.erase(m_lines.begin() + beg.line + 1, m_lines.begin() + end.line);
    {
    int line_num = 0;
    for (auto & line : m_lines)
        line.set_line_number(line_num++);
    }
    check_invarients();
    return beg;
}

std::u32string TextLines::copy_characters_from
    (Cursor beg, Cursor end) const
{
    verify_cursor_validity("TextLines::withdraw_characters_from (for beg)", beg);
    verify_cursor_validity("TextLines::withdraw_characters_from (for end)", end);
    std::u32string temp;
    auto collect_chars = [&temp, this](int line_idx, int line_beg, int line_end) {
        const auto & line = m_lines.at(std::size_t(line_idx));
        line.copy_characters_from(temp, line_beg, line_end);
        temp.push_back(NEW_LINE);
    };
    for_each_line_in_range(beg, end, collect_chars);
    temp.pop_back();
    return temp;
}

void TextLines::deposit_chatacters_to
    (UStringCIter beg, UStringCIter end, Cursor pos)
{
    if (beg == end) return;
    deposit_chatacters_to(&*beg, &*(end - 1) + 1, pos);
}

Cursor TextLines::deposit_chatacters_to
    (const UChar * beg, const UChar * end, Cursor pos)
{
    for (; beg != end; ++beg)
        pos = push(pos, *beg);
    return pos;
}

Cursor TextLines::next_cursor(Cursor cursor) const {
    verify_cursor_validity("TextLines::next_cursor", cursor);
    if (cursor == end_cursor()) return cursor;
    const auto & current_line = m_lines[std::size_t(cursor.line)];
    if (++cursor.column > current_line.content_length()) {
        cursor.column = 0;
        ++cursor.line;
    }
    return cursor;
}

Cursor TextLines::previous_cursor(Cursor cursor) const {
    verify_cursor_validity("TextLines::previous_cursor", cursor);
    if (--cursor.column < 0) {
        --cursor.line;
        if (cursor.line < 0)
            return Cursor();
        const auto & line = m_lines[std::size_t(cursor.line)];
        cursor.column = line.content_length();
    }
    return cursor;
}

Cursor TextLines::constrain_cursor(Cursor cursor) const noexcept {
    if (cursor == end_cursor()) return cursor;
    if (m_lines.empty()) return end_cursor();
    if (cursor.line < 0) {
        cursor.line = 0;
    } else if (!m_lines.empty() && cursor.line >= int(m_lines.size())) {
        cursor.line = int(m_lines.size()) - 1;
    }
    const auto & line = m_lines[std::size_t(cursor.line)];
    if (cursor.column < 0) {
        cursor.column = 0;
    } else if (cursor.column > line.content_length()) {
        cursor.column = line.content_length();
    }
    return cursor;
}

Cursor TextLines::end_cursor() const {
    return (m_lines.empty()) ? Cursor() : Cursor(int(m_lines.size()), 0);
}

bool TextLines::is_valid_cursor(Cursor cursor) const noexcept {
    if (cursor.line > int(m_lines.size())) return false;
    if (cursor.line == int(m_lines.size()))
        return cursor.column == 0;
    const auto & line = m_lines[std::size_t(cursor.line)];
    // the end of the line is a perfectly valid place to start typing
    return cursor.column <= int(line.content().length());
}

void TextLines::render_to(TargetTextGrid & target, int offset) const {
    for (const auto & line : m_lines) {
        line.render_to(target, offset);
        offset += line.height_in_cells();
    }
    if (offset > target.height()) return;
    Cursor cursor(std::max(0, offset), 0);
    assert(target.is_valid_cursor(cursor));
    const auto grid_end_c = target.end_cursor();
    const auto def_pair_c = m_rendering_options->get_default_pair();
    for (; cursor != grid_end_c; cursor = target.next_cursor(cursor)) {
        target.set_cell(cursor, U' ', def_pair_c);
    }
}

/* static */ void TextLines::run_tests() {
    do_text_lines_unit_tests();
}

template <typename Func>
/* private */ void TextLines::for_each_line_in_range
    (Cursor beg, Cursor end, Func && func) const
{
    assert(is_valid_cursor(beg));
    assert(is_valid_cursor(end));
    while (beg.line <= end.line && beg.line < int(m_lines.size())) {
        const auto & line = m_lines[std::size_t(beg.line)];
        int line_beg = beg.column;
        int line_end = beg.line == end.line ? end.column : line.content_length();
        func(beg.line, line_beg, line_end);
        beg.column = 0;
        ++beg.line;
    }
}

/* private */ void TextLines::check_invarients() const {
    ;
}

/* private */ void TextLines::verify_cursor_validity
    (const char * caller, Cursor cursor) const
{
    if (is_valid_cursor(cursor)) return;
    throw std::invalid_argument
        (std::string(caller) + ": given cursor is invalid.");
}

/* private */ void TextLines::update_ranges_impl(CodeModeler & modeler) {
    // important to skip redirection, otherwise infinite recursion!
    for (auto & line : m_lines)
        line.update_ranges_skip_redirection(modeler);
}

namespace {

void do_text_lines_unit_tests() {
    // push features
    auto push_string =
        [](TextLines * lines, const UChar * const str, Cursor cur)
    {
        for (auto c : std::u32string(str)) {
            cur = lines->push(cur, c);
        }
        return cur;
    };
    {
    TextLines tlines;
    Cursor c(0, 0);
    tlines.push(c, U' ');
    assert(tlines.end_cursor() == Cursor(1, 0));
    }
    {
    TextLines tlines;
    Cursor c(0, 0);
    tlines.push(c, TextLines::NEW_LINE);
    assert(tlines.end_cursor() == Cursor(2, 0));
    }
    {
    static constexpr const UChar * const howdy_neighor = U"Howdy neighbor";
    TextLines tlines;
    Cursor cur(0, 0);
    cur = tlines.push(cur, TextLines::NEW_LINE);
    cur = push_string(&tlines, howdy_neighor, cur);
    assert(Cursor(1, int(std::u32string(howdy_neighor).size())) == cur);
    }
    // copy_characters_from
    {
    static constexpr const UChar * const TEXT = U"Howdy Neighbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    assert(tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor()) == TEXT);
    }
    {
    static constexpr const UChar * const TEXT = U"Howdy\nNeighbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    const auto end_cursor_c = tlines.end_cursor();
    assert(tlines.copy_characters_from(Cursor(0, 0), end_cursor_c) == TEXT);
    }
    {
    static constexpr const UChar * const TEXT = U"How\ndy Nei\nghbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    assert(tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor()) == TEXT);
    }
    // delete ahead/behind
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_ahead(cur);
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"ab\nab" && cur == Cursor(0, 2));
    }
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_behind(cur);
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"ac\nab" && cur == Cursor(0, 1));
    }
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_ahead(tlines.delete_ahead(cur));
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"abab" && cur == Cursor(0, 2) && tlines.end_cursor().line == 1);
    }
    // wipe
    {
    TextLines tlines;
    push_string(&tlines, U"How now brown cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(0, 14));
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    {
    TextLines tlines;
    push_string(&tlines, U"How now bro\nwn cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(1, 3));
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    {
    TextLines tlines;
    push_string(&tlines, U"How now bro\nwn\nabc cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(2, 4));
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    // deposit_chatacters_to
    {
    static const std::u32string utext = U"Howdy Neighbor";
    TextLines tlines;
    tlines.deposit_chatacters_to(utext.data(), utext.data() + utext.length());
    auto ustr = tlines.copy_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == utext);
    }
    {
    NullTextGrid ntg;
    TextLines tlines;
    UserTextSelection uts;

    ntg.set_width(80);
    ntg.set_height(5);
    tlines.constrain_to_width(ntg.width());
    for (auto c : U"abc\ndef") {
        if (c == 0) continue;
        uts.push(&tlines, c);
    }
    for (int i = 0; i != 3; ++i)
        uts.move_left(tlines);
    // don't break invarients!
    uts.push(&tlines, TextLines::NEW_LINE);
    tlines.render_to(ntg, 0);
    }
    {
    TextLines tlines;
    assert(tlines.constrain_cursor(Cursor(10, 10)) == tlines.end_cursor());
    }
}

} // end of <anonymous> namespace
