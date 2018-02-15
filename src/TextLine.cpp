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

#include <limits>
#include <cassert>

namespace {

using UStringCIter = TextLines::UStringCIter;

TextLine::IteratorPair progress_through_line
    (TextLine::IteratorPair, UStringCIter end, const RenderOptions &,
     int max_width, int working_width);

bool is_newline(const TextLine::IteratorPair & ip)
    { return ip.begin == ip.end; }

bool is_whitespace(UChar uchr)
    { return uchr == U' ' || uchr == U'\n' || uchr == U'\t'; }

bool is_operator(UChar uchr);

bool is_neither_whitespace_or_operator(UChar uchr)
    { return !is_whitespace(uchr) && !is_operator(uchr); }

void run_text_line_tests();

void verify_text_line_content_string(const char * caller, const std::u32string &);

} // end of <anonymous> namespace

TextBreaker::~TextBreaker() {}

TextLine::TextLine():
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_rendering_options(&RenderOptions::get_default_instance())
{
    check_invarients();
}

TextLine::TextLine(const TextLine & rhs):
    m_grid_width(rhs.m_grid_width),
    m_extra_end_space(rhs.m_extra_end_space),
    m_content(rhs.m_content),
    m_rendering_options(rhs.m_rendering_options)
{
    update_ranges();
    check_invarients();
}

TextLine::TextLine(TextLine && rhs):
    TextLine()
{ swap(rhs); }

/* explicit */ TextLine::TextLine(const std::u32string & content_):
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_content(content_),
    m_rendering_options(&RenderOptions::get_default_instance())
{
    verify_text_line_content_string("TextLine::TextLine", content_);
    update_ranges();
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
    assign_render_options(RenderOptions::get_default_instance());
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

int TextLine::push(int column, UChar uchr) {
    verify_column_number("TextLine::push", column);
    if (uchr == TextLines::NEW_LINE) return SPLIT_REQUESTED;
    m_content.insert(m_content.begin() + column, 1, uchr);
    update_ranges();
    check_invarients();
    return column + 1;
}

int TextLine::delete_ahead(int column) {
    verify_column_number("TextLine::delete_ahead", column);
    if (column == int(m_content.size())) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column);
    update_ranges();
    check_invarients();
    return column;
}

int TextLine::delete_behind(int column) {
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

void TextLine::swap(TextLine & other) {
    std::swap(m_grid_width, other.m_grid_width);
    std::swap(m_extra_end_space, other.m_extra_end_space);

    m_row_ranges.swap(other.m_row_ranges);

    m_word_ranges.swap(other.m_word_ranges);
    m_content.swap(other.m_content);
    std::swap(m_rendering_options, other.m_rendering_options);
}

int TextLine::recorded_grid_width() const { return m_grid_width; }

int TextLine::height_in_cells() const {
    return 1 + int(m_row_ranges.size()) + m_extra_end_space;
}

const std::u32string & TextLine::content() const { return m_content; }

void TextLine::render_to
    (TargetTextGrid & target, int offset, int line_number) const
{
    if (m_grid_width != target.width()) {
        throw std::runtime_error(
            "TextLine::render_to: TextLine::constrain_to_width must be "
            "called with the correct width of the given text grid.");
    }

    if (m_content.empty()) {
        render_end_space(target, offset, line_number);
        return;
    }

    using IterPairIter = IteratorPairCIterator;
    auto process_row_ =
        [this, &target, &offset, &line_number]
        (IterPairIter word_itr, UStringCIter end)
    { return render_row(target, offset, word_itr, end, line_number); };

    const int original_offset_c = offset;
    auto row_begin = m_content.begin();
    auto cur_word_range = m_word_ranges.begin();
    for (auto row_end : m_row_ranges) {
        // do stuff with range
        cur_word_range = process_row_(cur_word_range, row_end);
        ++offset;
        row_begin = row_end;
    }
    cur_word_range = process_row_(cur_word_range, m_content.end());
    ++offset;
    assert(cur_word_range == m_word_ranges.end());
    render_end_space(target, original_offset_c, line_number);
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

/* private */ void TextLine::update_ranges() {
    m_word_ranges.clear();
    m_row_ranges.clear();
    if (m_content.empty()) return;

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
        // one of these word ranges is invalid
        assert(ip.begin >= m_content.begin() && ip.begin <  m_content.end());
        assert(ip.end   >= m_content.begin() && ip.end   <= m_content.end());
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
    if (offset >= target.height()) {
        return m_word_ranges.end();
    } else if (offset < 0) {
        for (; word_itr != m_word_ranges.end(); ++word_itr) {
            if (!word_itr->is_behind(row_end)) break;
        }
        return word_itr;
    }
    Cursor write_pos(offset, 0);
    assert(word_itr >= m_word_ranges.begin() && word_itr < m_word_ranges.end());
    for (; word_itr != m_word_ranges.end(); ++word_itr) {
        if (!word_itr->is_behind(row_end)) break;
        assert(word_itr->begin <= word_itr->end);
        auto color_pair = m_rendering_options->get_pair_for_word
            (std::u32string(word_itr->begin, word_itr->end));
        for (auto itr = word_itr->begin; itr != word_itr->end; ++itr) {
            assert(write_pos.column < m_grid_width);
            assert(itr >= m_content.begin() && itr < m_content.end());
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
    (TargetTextGrid & target, int offset, int line_number) const
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
        (Cursor(line_number, content_length()))(color_pair);
    target.set_cell(write_pos, U' ', color_pair);
    ++write_pos.column;
    fill_row_with_blanks(target, write_pos);
}

/* private */ void TextLine::check_invarients() const {
    assert(m_rendering_options);
    assert(m_extra_end_space == 0 || m_extra_end_space == 1);
    if (m_content.empty()) {
        assert(m_row_ranges .empty());
        assert(m_word_ranges.empty());
        return;
    }
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
        auto ip = *itr;

        assert(ip.begin >= m_content.begin() && ip.begin < m_content.end());
        assert(ip.end >= m_content.begin() && ip.end <= m_content.end());
        last = *itr;
    }
    }
}

namespace {

class DefaultBreaker final : public TextBreaker {
public:
    Response next_sequence(UStringCIter beg) override;
};

TextLine::IteratorPair progress_through_line
    (TextLine::IteratorPair pos, std::u32string::const_iterator end,
     const RenderOptions &, int max_width, int working_width)
{
    DefaultBreaker breaker;
    // the process of interpreting the meaning of the rv is not trivial
    using IteratorPair = TextLine::IteratorPair;
    // we should never encounter NEW_LINE!
    if (pos.begin == end || pos.end == end)
        return IteratorPair(end, end);

    auto resp = breaker.next_sequence(pos.end);
    auto next = resp.next;
    if (next == pos.end) return IteratorPair(end, end);

    TextLine::IteratorPair rv(pos.end, next);
    auto seq_length = rv.end - rv.begin;
    if (seq_length > working_width && seq_length <= max_width) {
        // whitespace is the exception, it does NOT wrap
        if (resp.is_whitespace) {//if (classifier_func == is_whitespace_c) {
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

bool is_operator(UChar uchr) {
    switch (uchr) {
    case U'`': case U'~': case U'!': case U'@': case U'$': case U'%':
    case U'^': case U'*': case U'(': case U')': case U'-': case U'=':
    case U'+':
    case U'[': case U']': case U'\\': case U';': case U'/': case U',':
    case U'.': case U'{': case U'}': case U'|': case U':': case U'"':
    case U'<': case U'>': case U'?': return true;
    default: return false;
    }
}

void verify_text_line_content_string(const char * caller, const std::u32string & content) {
    if (content.find(TextLines::NEW_LINE) != std::u32string::npos) {
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
    tline.render_to(ntg, 3, 0);
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
    tline.render_to(ntg, 0, 0);
    }
}

DefaultBreaker::Response DefaultBreaker::next_sequence(UStringCIter beg) {
    bool (*classifier_func)(UChar) = nullptr;
    auto check_and_set_classifier =
        [&classifier_func] (UChar u, bool (*f)(UChar))
    {
        if (!f(u)) return;
        assert(!classifier_func);
        classifier_func = f;
    };

    check_and_set_classifier(*beg, is_whitespace                    );
    check_and_set_classifier(*beg, is_operator                      );
    check_and_set_classifier(*beg, is_neither_whitespace_or_operator);
    assert(classifier_func);

    while (*beg != 0 && classifier_func(*beg)) {
        ++beg;
    }
    static constexpr bool (* const is_whitespace_c)(UChar) = is_whitespace;
    return Response { beg, is_whitespace_c == classifier_func };
}

} // end of <anonymous> namespace
