/****************************************************************************

    File: TextLine.hpp
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

#include <vector>
#include <string>
#include <stdexcept>

#include "Cursor.hpp"
#include "TargetTextGrid.hpp"

#pragma once

template <typename IterT>
class ConstIteratorPair {
public:
    using ValueType = decltype (*IterT());
    using IteratorType = IterT;
    ConstIteratorPair() {}
    ConstIteratorPair(IteratorType beg_, IteratorType end_):
        begin(beg_), end(end_)
    { if (!(beg_ <= end_)) throw std::invalid_argument("ConstIteratorPair::ConstIteratorPair: beg must be equal to or less than end."); }
    /** @warning Assumes that itr is from the same container as begin and end.
     */
    bool contains(IteratorType itr) const noexcept
        { return begin <= itr && itr < end; }
    bool is_behind(IteratorType itr) const noexcept
        { return end <= itr; }
    bool is_ahead(IteratorType itr) const noexcept
        { return itr < begin; }
    bool is_behind(const ConstIteratorPair & pair) const noexcept
        { return end <= pair.begin; }
    IteratorType begin, end;
};

// content string + extra ending space
class TextLine {
public:
    enum ContentTakingPlacement { PLACE_AT_BEGINING, PLACE_AT_END };
    static constexpr const int MERGE_REQUESTED = -1;
    static constexpr const int SPLIT_REQUESTED = -1;
    using UStringCIter = std::u32string::const_iterator;
    using IteratorPair = ConstIteratorPair<UStringCIter>;
    TextLine();
    TextLine(const TextLine &);
    TextLine(TextLine &&);
    explicit TextLine(const std::u32string &);

    TextLine & operator = (const TextLine &);
    TextLine & operator = (TextLine &&);

    // ------------------------ whole content editing -------------------------

    void constrain_to_width(int);
    void set_content(const std::u32string &);
    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_render_options(const RenderOptions &);
    void assign_default_render_options();
    TextLine split(int column);
    void take_contents_of(TextLine &, decltype(PLACE_AT_BEGINING));
    int wipe(int beg, int end);
    void copy_characters_from(std::u32string &, int beg, int end) const;
    int deposit_chatacters_to
        (UStringCIter beg, UStringCIter end, int pos);
    int deposit_chatacters_to(const UChar * beg, const UChar * end, int pos);

    void swap(TextLine &);

    // ----------------------- single character editing -----------------------

    // may return SPLIT_REQUESTED
    int push(int column, UChar);
    // may return MERGE_REQUESTED
    int delete_ahead(int column);
    // may return MERGE_REQUESTED
    int delete_behind(int column);

    // ------------------------------ accessors -------------------------------

    int recorded_grid_width() const;
    int height_in_cells() const;
    const std::u32string & content() const;
    int content_length() const { return int(content().length()); }

    void render_to(TargetTextGrid &, int offset, int line_number = 0) const;

    static void run_tests();
private:
    using IteratorPairCIterator = std::vector<IteratorPair>::const_iterator;
    void verify_column_number(const char * callername, int) const;
    void update_ranges();
    /**
     * @param target
     * @param offset row to render content to
     * @param word_itr first "word" to render, providing
     * @param row_end
     * @param line_number
     * @return Word on next row to render OR end iterator
     */
    IteratorPairCIterator render_row
        (TargetTextGrid & target, int offset, IteratorPairCIterator word_itr,
         UStringCIter row_end, int line_number) const;
    void fill_row_with_blanks(TargetTextGrid &, Cursor write_pos) const;
    void render_end_space(TargetTextGrid &, int offset, int line_number) const;
    void check_invarients() const;

    int m_grid_width;
    // required for edge case were all cells on the last row are occupied by
    // content, and therefore needing an extra space on the next so that the
    // user can type text at the end of the line
    int m_extra_end_space;
    // does not contain iterators begin and end in m_content
    std::vector<UStringCIter> m_row_ranges;
    // invarient -> cannot cross line range iterators
    std::vector<IteratorPair> m_word_ranges;
    std::u32string m_content;
    const RenderOptions * m_rendering_options;
};
