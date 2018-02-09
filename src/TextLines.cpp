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

#include <common/MultiType.hpp>

#include <limits>
#include <stdexcept>
#include <iostream>
#include <functional>

#include <cassert>

namespace {

const static auto default_rendering_options = RenderOptions();
using UStringCIter = TextLines::UStringCIter;

TextLine::IteratorPair progress_through_line
    (TextLine::IteratorPair, UStringCIter end, const RenderOptions &,
     int max_width, int working_width);

bool is_newline(const TextLine::IteratorPair & ip)
    { return ip.begin == ip.end; }

bool is_whitespace(UChar uchr) {
    return uchr == U' ' || uchr == U'\n' || uchr == U'\t';
}

bool is_whitespace(const TextLine::IteratorPair & ip) {
    if (ip.begin == ip.end) return false;
    return is_whitespace(*ip.begin);
}

bool is_operator(UChar uchr) {
    switch (uchr) {
    case U'`': case U'~': case U'!': case U'@': case U'$': case U'%':
    case U'^': case U'*': case U'(': case U')': case U'-': case U'=':
    case U'_': case U'+':
    case U'[': case U']': case U'\\': case U';': case U'/': case U',':
    case U'.': case U'{': case U'}': case U'|': case U':': case U'"':
    case U'<': case U'>': case U'?': return true;
    default: return false;
    }
}

bool is_operator(const TextLine::IteratorPair & ip) {
    if (ip.begin == ip.end) return false;
    return is_operator(*ip.begin);
}

bool is_neither_whitespace_or_operator(UChar uchr)
    { return !is_whitespace(uchr) && !is_operator(uchr); }

} // end of <anonymous> namespace

TextLines::TextLines():
    m_rendering_options(&default_rendering_options),
    m_width_constraint(std::numeric_limits<int>::max())
{}

/* explicit */ TextLines::TextLines(const std::u32string & content_):
    m_rendering_options(&default_rendering_options)
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
    assign_render_options(default_rendering_options);
}

Cursor TextLines::push(Cursor cursor, UChar uchar) {
    verify_cursor_validity("TextLines::push", cursor);
    if (cursor == end_cursor()) {
        m_lines.emplace_back();
        // need to tell the new line a few things
        m_lines.back().constrain_to_width(m_width_constraint);
        m_lines.back().assign_render_options(*m_rendering_options);
    }
    auto & line = m_lines[std::size_t(cursor.line)];
    auto resp = line.push(cursor.column, uchar);
    if (resp == TextLine::SPLIT_REQUESTED) {
        // if split is requested, the line has not been modified
        m_lines.insert
            (m_lines.begin() + cursor.line + 1, line.split(cursor.column));
        ++cursor.line;
        cursor.column = 0;
        return cursor;
    }
    // we know resp is a new column position now
    auto new_col = resp;
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
    int line_number = 0;
    for (const auto & line : m_lines) {
        line.render_to(target, offset, line_number++);
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
}

/* private */ void TextLines::verify_cursor_validity
    (const char * caller, Cursor cursor) const
{
    if (is_valid_cursor(cursor)) return;
    throw std::invalid_argument
        (std::string(caller) + ": given cursor is invalid.");
}

//
// ----------------------------------------------------------------------------
//

namespace {

void verify_text_line_content_string(const char * caller, const std::u32string &);

} // end of <anonymous> namespace

TextLine::TextLine():
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_rendering_options(&default_rendering_options)
{
    check_invarients();
}

/* explicit */ TextLine::TextLine(const std::u32string & content_):
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_content(content_),
    m_rendering_options(&default_rendering_options)
{
    verify_text_line_content_string("TextLine::TextLine", content_);
    update_ranges();
    check_invarients();
}

void TextLine::constrain_to_width(int target_width) {
    if (target_width < 1) {
        throw std::invalid_argument("TextLine::constrain_to_width:"
                                    "Grid width must be a positive integer.");
    }
    m_grid_width = target_width;
    update_ranges();
    check_invarients();
}

void TextLine::set_content(const std::u32string & content_) {
    verify_text_line_content_string("TextLine::set_content", content_);
    m_content = content_;
    update_ranges();
    check_invarients();
}

void TextLine::assign_render_options(const RenderOptions & options) {
    m_rendering_options = &options;
    update_ranges();
    check_invarients();
}

void TextLine::assign_default_render_options() {
    assign_render_options(default_rendering_options);
}

TextLine TextLine::split(int column) {
    verify_column_number("TextLine::split", column);
    auto new_line = TextLine(m_content.substr(std::size_t(column)));
    new_line.assign_render_options(*m_rendering_options);
    m_content.erase(m_content.begin() + column, m_content.end());
    new_line.constrain_to_width(recorded_grid_width());
    update_ranges();
    check_invarients();
    return new_line;
}

decltype (TextLine::SPLIT_REQUESTED) TextLine::push(int column, UChar uchr) {
    verify_column_number("TextLine::push", column);
    if (uchr == TextLines::NEW_LINE) return SPLIT_REQUESTED;
    m_content.insert(m_content.begin() + column, 1, uchr);
    update_ranges();
    check_invarients();
    return column + 1;
}

decltype (TextLine::MERGE_REQUESTED) TextLine::delete_ahead(int column) {
    verify_column_number("TextLine::delete_ahead", column);
    if (column == int(m_content.size())) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column);
    update_ranges();
    check_invarients();
    return column;
}

decltype (TextLine::MERGE_REQUESTED) TextLine::delete_behind(int column) {
    verify_column_number("TextLine::delete_behind", column);
    if (column == 0) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column - 1);
    update_ranges();
    check_invarients();
    return column - 1;
}

void TextLine::take_contents_of
    (TextLine & other_line, ContentTakingPlacement place)
{
    if (place == PLACE_AT_END) {
        m_content += other_line.content();
    } else {
        assert(place == PLACE_AT_BEGINING);
        m_content.insert(m_content.begin(), other_line.content().begin(),
                         other_line.content().end()                     );
    }
    other_line.wipe(0, other_line.content_length());
    update_ranges();
    check_invarients();
}

int TextLine::wipe(int beg, int end) {
    verify_column_number("TextLine::wipe (for beg)", beg);
    verify_column_number("TextLine::wipe (for end)", end);
    m_content.erase(m_content.begin() + beg, m_content.begin() + end);
    update_ranges();
    check_invarients();
    return int(m_content.length());
}

void TextLine::copy_characters_from
    (std::u32string & dest, int beg, int end) const
{
    verify_column_number("TextLine::copy_characters_from (for beg)", beg);
    verify_column_number("TextLine::copy_characters_from (for end)", end);
    dest.append(m_content.begin() + beg, m_content.begin() + end);
}

int TextLine::deposit_chatacters_to
    (UStringCIter beg, UStringCIter end, int pos)
{
    verify_column_number("TextLine::deposit_chatacters_to", pos);
    if (beg == end) return pos;
    return deposit_chatacters_to(&*beg, &*(end - 1) + 1, pos);
}

int TextLine::deposit_chatacters_to
    (const UChar * beg, const UChar * end, int pos)
{
    verify_column_number("TextLine::deposit_chatacters_to", pos);
    if (beg == end) return pos;
    m_content.insert(m_content.begin() + pos, beg, end);
    update_ranges();
    check_invarients();
    return pos + int(end - beg);
}

int TextLine::recorded_grid_width() const { return m_grid_width; }

int TextLine::height_in_cells() const {
    return 1 + int(m_row_ranges.size()) + m_extra_end_space;
}

const std::u32string & TextLine::content() const { return m_content; }

void TextLine::render_to
    (TargetTextGrid & target, int offset, int line_number) const
{
    if (m_content.empty()) return;
    auto row_begin = m_content.begin();
    auto cur_word_range = m_word_ranges.begin();
    using IterPairIter = IteratorPairCIterator;
    auto process_row_o =
        [this, &target, &offset, &line_number]
        (IterPairIter word_itr, UStringCIter end)
    { return render_row(target, offset++, word_itr, end, line_number); };

    for (auto row_end : m_row_ranges) {
        // do stuff with range
        cur_word_range = process_row_o(cur_word_range, row_end);
        row_begin = row_end;
    }
    cur_word_range = process_row_o(cur_word_range, m_content.end());
    Cursor write_pos(offset + int(m_row_ranges.size()), 0);
    if (m_extra_end_space == 1) {
        write_pos.column = 0;
    } else if (m_row_ranges.empty()) {
        write_pos.column = content_length();
        --write_pos.line;
    } else {
        write_pos.column = int(m_content.end() - m_row_ranges.back()) + 1;
        --write_pos.line;
    }
    auto color_pair = m_rendering_options->get_default_pair();
    if (m_rendering_options->color_adjust_for
            (Cursor(line_number, content_length())) == RenderOptions::invert)
    {
        int j = 0;
        ++j;
    }
    color_pair = m_rendering_options->color_adjust_for
        (Cursor(line_number, content_length()))(color_pair);
    target.set_cell(write_pos, U' ', color_pair);
    ++write_pos.column;
    fill_row_with_blanks(target, write_pos);
    assert(cur_word_range == m_word_ranges.end());
}

/* static */ void TextLine::run_tests() {
    {
    TextLine tline(U" ");
    assert(tline.height_in_cells() == 1);
    }
    {
    // hard wrapping
    TextLine tline(U"0123456789");
    tline.constrain_to_width(8);
    assert(tline.height_in_cells() == 2);
    }
    {
    // soft wrapping
    TextLine tline(U"function do_something() return \"Hello world\" end");
    tline.constrain_to_width(31);
    assert(tline.height_in_cells() == 2);
    }
    // split
    {
    TextLine tline(U"0123456789");
    auto other_tline = tline.split(5);
    assert(other_tline.content() == U"56789");
    }
    // split on the beginning
    {
    TextLine tline(U"0123456789");
    auto other_tline = tline.split(0);
    assert(other_tline.content() == U"0123456789");
    }
    // take_contents_of
    {
    TextLine tline(U"01234");
    TextLine otline(U"56789");
    tline.take_contents_of(otline, TextLine::PLACE_AT_END);
    assert(tline.content() == U"0123456789" && otline.content_length() == 0);
    }
    // wipe
    // copy_characters_from
    // deposit_chatacters_to
}

/* private */ void TextLine::verify_column_number
    (const char * callername, int column) const
{
    if (column > -1 && column <= int(m_content.size())) return;
    throw std::invalid_argument
        (std::string(callername) + ": given column number is invalid.");
}

/* private */ void TextLine::update_ranges() {
    if (m_content.empty()) return;
    m_word_ranges.clear();
    m_row_ranges.clear();
    // this part is hard...
    // I need actual iterators, since I'm recording them
    IteratorPair ip(m_content.begin(), m_content.begin());
    auto next_pair = [this](IteratorPair ip, int working_width) {
        return progress_through_line(ip, m_content.end(), *m_rendering_options,
                                     m_grid_width, working_width);
    };
    int working_width = m_grid_width;
    for (ip = next_pair(ip, working_width); ip.begin != m_content.end();
         ip = next_pair(ip, working_width))
    {
        if (is_newline(ip)) {
            m_row_ranges.push_back(ip.begin);
            working_width = m_grid_width;
        } else {
            // "words" here are not words (renaming necessary)
            m_word_ranges.push_back(ip);
            working_width -= (ip.end - ip.begin);
        }
    }
    m_extra_end_space = (working_width == 0) ? 1 : 0;
    check_invarients();
}

/* private */ TextLine::IteratorPairCIterator TextLine::render_row
    (TargetTextGrid & target, int offset, IteratorPairCIterator word_itr,
     UStringCIter row_end, int line_number) const
{
    Cursor write_pos(offset, 0);
    for (; word_itr->is_behind(row_end) && word_itr != m_word_ranges.end();
         ++word_itr)
    {
        if (offset < 0) continue;
        assert(word_itr->begin <= word_itr->end);
        auto color_pair = m_rendering_options->get_pair_for_word
            (std::u32string(word_itr->begin, word_itr->end));
        for (auto itr = word_itr->begin; itr != word_itr->end; ++itr) {
            assert(write_pos.column < m_grid_width);
            Cursor text_pos(line_number, int(itr - m_content.begin()));
            auto char_cpair = m_rendering_options->
                color_adjust_for(text_pos)(color_pair);
            target.set_cell(write_pos, *itr, char_cpair);
            if (*itr == U'\t')
                write_pos.column += m_rendering_options->tab_width();
            else
                ++write_pos.column;
        }
    }
    if (offset < 0) return word_itr;
    // fill rest of grid row
#   if 0
    if (m_extra_end_space == 0) {
        auto color_pair = m_rendering_options->get_default_pair();
        Cursor end_cursor(line_number, content_length());
        color_pair = m_rendering_options->color_adjust_for(end_cursor)(color_pair);
        target.set_cell(write_pos, U' ', color_pair);
        ++write_pos.column;
    }
#   endif
    fill_row_with_blanks(target, write_pos);
    return word_itr;
}

/* private */ void TextLine::fill_row_with_blanks
    (TargetTextGrid & target, Cursor write_pos) const
{
    auto def_pair = m_rendering_options->get_default_pair();
    for (; write_pos.column != m_grid_width; ++write_pos.column) {
        target.set_cell(write_pos, U' ', def_pair);
    }
}

/* private */ void TextLine::check_invarients() const {
    assert(m_rendering_options);
    assert(m_extra_end_space == 0 || m_extra_end_space == 1);
    if (m_content.empty()) return;
    if (!m_row_ranges.empty()) {
        auto last = m_row_ranges.front();
        auto itr  = m_row_ranges.begin() + 1;
        auto word_itr = m_word_ranges.begin();
        for (; itr != m_row_ranges.end(); ++itr) {
            assert(last < *itr);
            assert(*itr - last <= m_grid_width);
            while (word_itr != m_word_ranges.end()) {
                if (word_itr->is_behind(*itr)) {
                    ++word_itr;
                    continue;
                } else {
                    //assert(!word_itr->contains(*itr));
                    break;
                }
            }
            last = *itr;
        }
    }
    {
    if (m_word_ranges.empty()) return;
    auto last = m_word_ranges.front();
    auto itr  = m_word_ranges.begin() + 1;
    for (; itr != m_word_ranges.end(); ++itr) {
        assert(last.is_behind(*itr));
        last = *itr;
    }
    }
}

namespace {

TextLine::IteratorPair progress_through_line
    (TextLine::IteratorPair pos, UStringCIter end,
     const RenderOptions & options, int max_width, int working_width)
{
    // the process of interpreting the meaning of the rv is not trivial
    using IteratorPair = TextLine::IteratorPair;
    // we should never encounter NEW_LINE!
    if (pos.begin == end) return pos;
    if (pos.end   == end) return IteratorPair(end, end);

    bool (*classifier_func)(UChar) = nullptr;
    auto check_and_set_classifier =
        [&classifier_func] (UChar u, bool (*f)(UChar))
    {
        if (!f(u)) return;
        assert(!classifier_func);
        classifier_func = f;
    };

    auto new_end = pos.end;
    check_and_set_classifier(*new_end, is_whitespace                    );
    check_and_set_classifier(*new_end, is_operator                      );
    check_and_set_classifier(*new_end, is_neither_whitespace_or_operator);
    assert(classifier_func);
    int space_required = 0;
    while (new_end != end && classifier_func(*new_end)) {
        if (*new_end == U'\t')
            space_required += options.tab_width();
        else
            ++space_required;
        ++new_end;
    }
    TextLine::IteratorPair rv(pos.end, new_end);
    auto seq_length = rv.end - rv.begin;
    static constexpr bool (* const is_whitespace_c)(UChar) = is_whitespace;
    if (seq_length > working_width && seq_length <= max_width) {
        // whitespace is the exception, it does NOT wrap
        if (classifier_func == is_whitespace_c) {
            return IteratorPair(rv.begin, rv.begin + working_width);
        } else {
            // soft wrap, "ask" for a reset of working space
            return IteratorPair(rv.begin, rv.begin);
        }
    } else if (seq_length > max_width) {
        // hard wrap, full available width
        return IteratorPair(rv.begin, rv.begin + working_width);
    } else {
        return rv;
    }
}

} // end of <anonymous> namespace

namespace {

void verify_text_line_content_string(const char * caller, const std::u32string & content) {
    if (content.find(TextLines::NEW_LINE) != std::u32string::npos) {
        throw std::invalid_argument
            (std::string(caller) + ": content string may not contain a new line.");
    }
}

} // end of <anonymous> namespace
