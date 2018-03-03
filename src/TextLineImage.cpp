/****************************************************************************

    File: TextLineImage.cpp
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

#include "TextLineImage.hpp"

#include <limits>

#include <cassert>

namespace {

void run_text_line_image_tests();

} // end of <anonymous> namespace

TextLineImage::TextLineImage():
    m_grid_width(std::numeric_limits<int>::max()),
    m_extra_end_space(0),
    m_rendering_options(&RenderOptions::get_default_instance()),
    m_line_number(NO_LINE_NUMBER)
{}

TextLineImage::TextLineImage(TextLineImage && rhs) { swap(rhs); }

TextLineImage & TextLineImage::operator = (TextLineImage && rhs) {
    swap(rhs);
    return *this;
}

void TextLineImage::update_modeler
    (CodeModeler & modeler, const std::u32string & string)
{
    update_modeler(modeler, string.begin(), string.end());
}

void TextLineImage::update_modeler
    (CodeModeler & modeler, UStringCIter beg, UStringCIter end)
{
    clear_image();
    int working_width = m_grid_width;
    for (UStringCIter itr = beg; itr != end;) {
        assert(*itr);
        assert(itr != end);
        int col = int(itr - beg);
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
                         Cursor(m_line_number, int(end - beg)));
    m_extra_end_space = (working_width == 0) ? 1 : 0;
    check_invarients();
}

void TextLineImage::clear_image() {
    m_tokens.clear();
    m_row_ranges.clear();
    m_extra_end_space = 0;
}

int TextLineImage::height_in_cells() const {
    return 1 + int(m_row_ranges.size()) + m_extra_end_space;
}

void TextLineImage::assign_render_options(const RenderOptions & options) {
    m_rendering_options = &options;
}

void TextLineImage::render_to(TargetTextGrid & target, int offset) const {
    if (m_grid_width != target.width()) {
        throw std::runtime_error(
            "TextLine::render_to: TextLine::constrain_to_width must be "
            "called with the correct width of the given text grid.");
    }

    if (m_tokens.empty()) {
        render_end_space(target, offset);
        return;
    }

    using IterPairIter = decltype (m_tokens.begin());
    auto process_row_ =
        [this, &target, &offset]
        (IterPairIter word_itr, UStringCIter end)
    { return render_row(target, offset, word_itr, end); };

    const int original_offset_c = offset;
    auto row_begin = m_tokens.front().pair.begin();
    auto cur_word_range = m_tokens.begin();
    for (auto row_end : m_row_ranges) {
        // do stuff with range
        cur_word_range = process_row_(cur_word_range, row_end);
        ++offset;
        row_begin = row_end;
    }
    cur_word_range = process_row_(cur_word_range, m_tokens.back().pair.end());
    ++offset;
    assert(cur_word_range == m_tokens.end());
    render_end_space(target, original_offset_c);
}

void TextLineImage::swap(TextLineImage & other) {
    std::swap(m_grid_width, other.m_grid_width);
    std::swap(m_extra_end_space, other.m_extra_end_space);
    m_row_ranges.swap(other.m_row_ranges);
    std::swap(m_rendering_options, other.m_rendering_options);
    std::swap(m_line_number, other.m_line_number);
    m_tokens.swap(other.m_tokens);

    check_invarients();
    other.check_invarients();
}

void TextLineImage::copy_rendering_details(const TextLineImage & rhs) {
    m_grid_width = rhs.m_grid_width;
    m_rendering_options = rhs.m_rendering_options;
    check_invarients();
}

void TextLineImage::constrain_to_width(int target_width) {
    if (target_width < 1) {
        throw std::invalid_argument("TextLineImage::constrain_to_width: "
                                    "Grid width must be a positive integer.");
    }
    m_grid_width = target_width;
    check_invarients();
}

void TextLineImage::set_line_number(int line_number) {
    if (line_number != NO_LINE_NUMBER && line_number < 0) {
        throw std::invalid_argument(
            "TextLineImage::set_line_number: Code line number may only a "
            "non-negative integer (with the exception of sentinel values).");
    }
    m_line_number = line_number;
    check_invarients();
}

/* static */ void TextLineImage::run_tests() { run_text_line_image_tests(); }

/* private */ TextLineImage::TokenInfoCIter TextLineImage::render_row
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
    assert(word_itr >= m_tokens.begin() && word_itr < m_tokens.end());
    for (; word_itr != m_tokens.end(); ++word_itr) {
        if (!word_itr->pair.is_behind(row_end)) break;
        assert(word_itr->pair.begin() <= word_itr->pair.end());
        auto color_pair = m_rendering_options->get_pair_for_token_type(word_itr->type);
        for (const auto & chr : word_itr->pair) {
            assert(write_pos.column < m_grid_width);
            auto content_begin = &*m_tokens.front().pair.begin();
            Cursor text_pos(m_line_number, int(&chr - content_begin));
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

/* private */ void TextLineImage::fill_row_with_blanks
    (TargetTextGrid & target, Cursor write_pos) const

{
    auto def_pair = m_rendering_options->get_default_pair();
    for (; write_pos.column != m_grid_width; ++write_pos.column) {
        target.set_cell(write_pos, U' ', def_pair);
    }
}

/* private */ void TextLineImage::render_end_space
    (TargetTextGrid & target, int offset) const
{
    Cursor write_pos(offset + height_in_cells() - 1, 0);
    int content_len = 0;
    if (!m_tokens.empty())
        content_len = int(m_tokens.back().pair.end() - m_tokens.front().pair.begin());
    assert(content_len >= 0);
    if (m_extra_end_space == 1) {
        write_pos.column = 0;
    } else if (m_row_ranges.empty()) {
        write_pos.column = content_len;
    } else {
        write_pos.column = int(m_tokens.back().pair.end() - m_row_ranges.back());
    }
    if (write_pos.line >= target.height() || write_pos.line < 0) return;
    auto color_pair = m_rendering_options->get_default_pair();
    color_pair = m_rendering_options->color_adjust_for
        (Cursor(m_line_number, content_len))(color_pair);
    target.set_cell(write_pos, U' ', color_pair);
    ++write_pos.column;
    fill_row_with_blanks(target, write_pos);
}

/* private */ int TextLineImage::handle_hard_wraps
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
}

/* private */ void TextLineImage::check_invarients() const {
    assert(m_rendering_options);
    assert(m_extra_end_space == 0 || m_extra_end_space == 1);

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
    if (!m_tokens.empty()) {
        auto last = m_tokens.front();
        auto itr  = m_tokens.begin() + 1;
        for (; itr != m_tokens.end(); ++itr) {
            assert(last.pair.is_behind(itr->pair));
            last = *itr;
        }
    }
}

namespace {

void run_text_line_image_tests() {

}

} // end of <anonymous> namespace
