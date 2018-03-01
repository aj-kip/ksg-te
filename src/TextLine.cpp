/****************************************************************************

    File: TextLine.cpp
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

#include "TextLine.hpp"
#include "TextLines.hpp"
#include "LuaCodeModeler.hpp"

#include <limits>
#include <cassert>

namespace {

using UStringCIter = TextLines::UStringCIter;

bool is_whitespace(UChar uchr)
    { return uchr == U' ' || uchr == U'\n' || uchr == U'\t'; }

void run_text_line_tests();

void verify_text_line_content_string(const char * caller, const std::u32string &);

class DefaultCodeModeler final : public CodeModeler {
    void reset_state() override {}
    Response update_model(UStringCIter itr, Cursor) override {
        bool is_ws = is_whitespace(*itr);
        for (; *itr && is_ws == is_whitespace(*itr); ++itr) {}
        auto tok_type = is_ws && (*itr == 0 || *itr == TextLines::NEW_LINE) ? LEADING_WHITESPACE : REGULAR_SEQUENCE;
        return Response { itr, tok_type, is_ws };
    }
};

// assumes certain optimizations are present, which for supported platforms
// are ok
static_assert(sizeof(DefaultCodeModeler) - sizeof(CodeModeler) == 0, "");

} // end of <anonymous> namespace

RangesUpdater::RangesUpdater(const RangesUpdater & rhs):
    RangesUpdater()
{
    if (rhs.m_observer != &rhs)
        m_observer = rhs.m_observer;
}

RangesUpdater & RangesUpdater::operator = (const RangesUpdater & rhs) {
    if (rhs.m_observer != &rhs)
        m_observer = rhs.m_observer;
    return *this;
}

RangesUpdater::~RangesUpdater() {}

void RangesUpdater::update_ranges(CodeModeler & model) {
    assert(m_observer);
    m_observer->update_ranges_impl(model);
}

void RangesUpdater::update_ranges_skip_redirection(CodeModeler & model) {
    update_ranges_impl(model);
}

void RangesUpdater::assign_ranges_updater(RangesUpdater & target) {
    m_observer = &target;
}

void RangesUpdater::reset_ranges_updater_to_this() {
    m_observer = this;
}

CodeModeler::~CodeModeler() {}

/* static */ CodeModeler & CodeModeler::default_instance() {
    static DefaultCodeModeler instance;
    return instance;
}

TextLine::TextLine():
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_rendering_options(&RenderOptions::get_default_instance()),
    m_code_modeler(&CodeModeler::default_instance()),
    m_line_number(NO_LINE_NUMBER)
{
    check_invarients();
}

TextLine::TextLine(const TextLine & rhs):
    RangesUpdater(),
    m_grid_width(rhs.m_grid_width),
    m_extra_end_space(rhs.m_extra_end_space),
    m_content(rhs.m_content),
    m_rendering_options(rhs.m_rendering_options),
    m_code_modeler(rhs.m_code_modeler),
    m_line_number(rhs.m_line_number)
{
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

TextLine::TextLine(TextLine && rhs):
    TextLine()
{ swap(rhs); }

/* explicit */ TextLine::TextLine(const std::u32string & content_):
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_content(content_),
    m_rendering_options(&RenderOptions::get_default_instance()),
    m_code_modeler(&CodeModeler::default_instance()),
    m_line_number(NO_LINE_NUMBER)
{
    verify_text_line_content_string("TextLine::TextLine", content_);
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

TextLine & TextLine::operator = (const TextLine & rhs) {
    if (&rhs != this) {
        TextLine temp(rhs);
        swap(temp);
    }
    return *this;
}

TextLine & TextLine::operator = (TextLine && rhs) {
    if (&rhs != this) swap(rhs);
    return *this;
}

void TextLine::constrain_to_width(int target_width) {
    if (target_width < 1) {
        throw std::invalid_argument("TextLine::constrain_to_width:"
                                    "Grid width must be a positive integer.");
    }
    m_grid_width = target_width;
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

void TextLine::set_content(const std::u32string & content_) {
    verify_text_line_content_string("TextLine::set_content", content_);
    m_content = content_;
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

void TextLine::assign_render_options
    (const RenderOptions & options)
{
    m_rendering_options = &options;
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

void TextLine::set_line_number(int line_number) {
    if (line_number != NO_LINE_NUMBER && line_number < 0) {
        throw std::invalid_argument(
            "TextLine::set_line_number: Code line number may only a "
            "non-negative integer (with the exception of sentinel values).");
    }
    m_line_number = line_number;
    check_invarients();
}

void TextLine::assign_default_render_options() {
    assign_render_options(RenderOptions::get_default_instance());
}

TextLine TextLine::split(int column) {
    verify_column_number("TextLine::split", column);
    auto new_line = TextLine(m_content.substr(std::size_t(column)));
    new_line.assign_render_options(*m_rendering_options);
    m_content.erase(m_content.begin() + column, m_content.end());
    new_line.constrain_to_width(recorded_grid_width());
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
    return new_line;
}

int TextLine::push(int column, UChar uchr) {
    verify_column_number("TextLine::push", column);
    if (uchr == TextLines::NEW_LINE) return SPLIT_REQUESTED;
    verify_text("TextLine::push", uchr);
    m_content.insert(m_content.begin() + column, 1, uchr);
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
    return column + 1;
}

int TextLine::delete_ahead(int column) {
    verify_column_number("TextLine::delete_ahead", column);
    if (column == int(m_content.size())) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column);
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
    return column;
}

int TextLine::delete_behind(int column) {
    verify_column_number("TextLine::delete_behind", column);
    if (column == 0) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column - 1);
    LuaCodeModeler lcm;
    update_ranges(lcm);
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
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
}

int TextLine::wipe(int beg, int end) {
    verify_column_number("TextLine::wipe (for beg)", beg);
    verify_column_number("TextLine::wipe (for end)", end);
    m_content.erase(m_content.begin() + beg, m_content.begin() + end);
    LuaCodeModeler lcm;
    update_ranges(lcm);
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
    verify_text("TextLine::deposit_chatacters_to", beg, end);
    if (beg == end) return pos;
    m_content.insert(m_content.begin() + pos, beg, end);
    LuaCodeModeler lcm;
    update_ranges(lcm);
    check_invarients();
    return pos + int(end - beg);
}

void TextLine::swap(TextLine & other) {
    std::swap(m_grid_width, other.m_grid_width);
    std::swap(m_extra_end_space, other.m_extra_end_space);
    m_row_ranges.swap(other.m_row_ranges);
    m_content   .swap(other.m_content   );
    std::swap(m_rendering_options, other.m_rendering_options);
    std::swap(m_code_modeler, other.m_code_modeler);
    std::swap(m_line_number, other.m_line_number);
    m_tokens.swap(other.m_tokens);

    check_invarients();
    other.check_invarients();
}

int TextLine::recorded_grid_width() const { return m_grid_width; }

int TextLine::height_in_cells() const {
    return 1 + int(m_row_ranges.size()) + m_extra_end_space;
}

const std::u32string & TextLine::content() const { return m_content; }

void TextLine::render_to(TargetTextGrid & target, int offset) const {
    if (m_grid_width != target.width()) {
        throw std::runtime_error(
            "TextLine::render_to: TextLine::constrain_to_width must be "
            "called with the correct width of the given text grid.");
    }

    if (m_content.empty()) {
        render_end_space(target, offset);
        return;
    }

    using IterPairIter = decltype (m_tokens.begin());
    auto process_row_ =
        [this, &target, &offset]
        (IterPairIter word_itr, UStringCIter end)
    { return render_row(target, offset, word_itr, end); };

    const int original_offset_c = offset;
    auto row_begin = m_content.begin();
    auto cur_word_range = m_tokens.begin();
    for (auto row_end : m_row_ranges) {
        // do stuff with range
        cur_word_range = process_row_(cur_word_range, row_end);
        ++offset;
        row_begin = row_end;
    }
    cur_word_range = process_row_(cur_word_range, m_content.end());
    ++offset;
    assert(cur_word_range == m_tokens.end());
    render_end_space(target, original_offset_c);
}

/* static */ void TextLine::run_tests() {
    run_text_line_tests();
}

/* private */ void TextLine::verify_column_number
    (const char * callername, int column) const
{
    if (column > -1 && column <= int(m_content.size())) return;
    throw std::invalid_argument
        (std::string(callername) + ": given column number is invalid.");
}

/* private */ void TextLine::verify_text
    (const char * callername, UChar uchr) const
{
    if (uchr != TextLines::NEW_LINE && uchr != 0) return;
    throw std::invalid_argument
        (std::string(callername) + ": input characters for TextLine must not "
         "be TextLines::NEW_LINE or the null terminator."                     );
}

/* private */ void TextLine::verify_text
    (const char * callername, const UChar * beg, const UChar * end) const
{
    assert(end >= beg);
    for (auto itr = beg; itr != end; ++itr)
        verify_text(callername, *itr);
}

/* private */ void TextLine::update_ranges_impl(CodeModeler & modeler) {
    m_tokens.clear();
    m_row_ranges.clear();
    if (m_content.empty()) return;

    auto handle_hard_wraps = [this]
        (const CodeModeler::Response & resp, UStringCIter itr, int working_width)
    {
        assert(resp.next - itr > working_width);
        const auto grid_width_c = m_grid_width;
        auto mid = itr + working_width;
        while (true) {
            m_tokens.emplace_back(resp.token_type, itr, mid);
            m_row_ranges.push_back(mid);
            itr = mid;
            if (resp.next - mid > grid_width_c)
                mid += grid_width_c;
            else
                break;
        }
        m_tokens.emplace_back(resp.token_type, mid, resp.next);
        return m_grid_width - int(resp.next - mid);
    };
    int working_width = m_grid_width;
    for (UStringCIter itr = m_content.begin(); itr != m_content.end();) {
        assert(*itr);
        assert(itr != m_content.end());
        int col = int(itr - m_content.begin());
        const auto resp = modeler.update_model(itr, Cursor(m_line_number, col));
        const auto seq_len = resp.next - itr;
        assert(seq_len != 0);

        // forced split
        if (seq_len > m_grid_width) {
            working_width = handle_hard_wraps(resp, itr, working_width);
        }
        // split
        else if (resp.always_hardwrap && seq_len > working_width) {
            working_width = handle_hard_wraps(resp, itr, working_width);
        }
        // flow over
        else if (!resp.always_hardwrap && seq_len > working_width) {
            m_row_ranges.push_back(itr);
            m_tokens.emplace_back(resp.token_type, itr, resp.next);
            working_width = m_grid_width - int(seq_len);
        }
        // regular write
        else {
            m_tokens.emplace_back(resp.token_type, itr, resp.next);
            working_width -= seq_len;
        }
        itr = resp.next;
    }
    static const std::u32string NEW_LINE = U"\n";
    modeler.update_model(NEW_LINE.begin(),
                         Cursor(m_line_number, int(m_content.size())));
    m_extra_end_space = (working_width == 0) ? 1 : 0;
    check_invarients();
}

/* private */ TextLine::TokenInfoCIter TextLine::render_row
    (TargetTextGrid & target, int offset, TokenInfoCIter word_itr,
     UStringCIter row_end) const
{
    if (offset >= target.height()) {
        return m_tokens.end();
    } else if (offset < 0) {
        for (; word_itr != m_tokens.end(); ++word_itr) {
            if (!word_itr->pair.is_behind(row_end)) break;
        }
        return word_itr;
    }
    Cursor write_pos(offset, 0);
    //LuaHighlighter luahtr;
    assert(word_itr >= m_tokens.begin() && word_itr < m_tokens.end());
    for (; word_itr != m_tokens.end(); ++word_itr) {
        if (!word_itr->pair.is_behind(row_end)) break;
        assert(word_itr->pair.begin() <= word_itr->pair.end());
        assert(word_itr->pair.begin() >= m_content.begin() &&
               word_itr->pair.end() <= m_content.end());
        auto color_pair = m_rendering_options->get_pair_for_token_type(word_itr->type);
        for (const auto & chr : word_itr->pair) {
            assert(write_pos.column < m_grid_width);
            Cursor text_pos(m_line_number, int(&chr - &m_content.front()));
            auto char_cpair = m_rendering_options->
                color_adjust_for(text_pos)(color_pair);
            target.set_cell(write_pos, chr, char_cpair);
            if (chr == U'\t')
                write_pos.column += m_rendering_options->tab_width();
            else
                ++write_pos.column;
        }
    }

    // fill rest of grid row
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

/* private */ void TextLine::render_end_space
    (TargetTextGrid & target, int offset) const
{
    Cursor write_pos(offset + height_in_cells() - 1, 0);
    if (m_extra_end_space == 1) {
        write_pos.column = 0;
    } else if (m_row_ranges.empty()) {
        write_pos.column = content_length();
    } else {
        write_pos.column = int(m_content.end() - m_row_ranges.back());
    }
    if (write_pos.line >= target.height() || write_pos.line < 0) return;
    auto color_pair = m_rendering_options->get_default_pair();
    color_pair = m_rendering_options->color_adjust_for
        (Cursor(m_line_number, content_length()))(color_pair);
    target.set_cell(write_pos, U' ', color_pair);
    ++write_pos.column;
    fill_row_with_blanks(target, write_pos);
}

/* private */ void TextLine::check_invarients() const {
    assert(m_rendering_options);
    assert(m_code_modeler);
    assert(m_extra_end_space == 0 || m_extra_end_space == 1);
    if (m_content.empty()) {
        assert(m_row_ranges.empty());
        assert(m_tokens    .empty());
        return;
    } else {
        assert(!m_tokens.empty());
    }
    if (!m_row_ranges.empty()) {
        auto last = m_row_ranges.front();
        auto itr  = m_row_ranges.begin() + 1;
        auto word_itr = m_tokens.begin();
        for (; itr != m_row_ranges.end(); ++itr) {
            assert(last < *itr);
            assert(*itr - last <= m_grid_width);
            while (word_itr != m_tokens.end()) {
                if (word_itr->pair.is_behind(*itr)) {
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
    if (m_tokens.empty()) return;
    auto last = m_tokens.front();
    auto itr  = m_tokens.begin() + 1;
    for (; itr != m_tokens.end(); ++itr) {
        assert(last.pair.is_behind(itr->pair));
        auto ip = itr->pair;
        assert(ip.begin() >= m_content.begin() && ip.begin() < m_content.end());
        assert(ip.end() >= m_content.begin() && ip.end() <= m_content.end());
        last = *itr;
    }
    }
}

namespace {

void verify_text_line_content_string(const char * caller, const std::u32string & content) {
    if (content.find(TextLines::NEW_LINE) != std::u32string::npos ||
        content.find(           UChar(0)) != std::u32string::npos   )
    {
        throw std::invalid_argument
            (std::string(caller) + ": content string may not contain a new line.");
    }
}

void run_text_line_tests() {
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
    {
    NullTextGrid ntg;
    TextLine tline;
    int i = 0;
    tline.push(i++, U'-');
    tline.push(i++, U'-');
    tline.push(i++, TextLines::NEW_LINE);
    tline.split(i - 1);
    ntg.set_width (80);
    ntg.set_height(30);
    tline.constrain_to_width(ntg.width());
    tline.render_to(ntg, 3);
    }
    {
    NullTextGrid ntg;
    UserTextSelection uts;
    TextLine tline;
    ntg.set_width(80);
    ntg.set_height(3);
    tline.constrain_to_width(ntg.width());
    int pos = 0;
    for (int i = 0; i != (80 + 79); ++i) {
        pos = tline.push(pos, U'a');
    }
    tline.set_line_number(0);
    tline.render_to(ntg, 0);
    }
}

} // end of <anonymous> namespace
