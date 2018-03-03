/****************************************************************************

    File: TextLineImage.hpp
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

#pragma once

#include "TargetTextGrid.hpp"
#include "IteratorPair.hpp"

#include <string>
#include <vector>

// multi line "objects" make this especially difficult...
// namely C's multiline comments, Lua's multiline strings
class CodeModeler {
public:
    using UStringCIter = std::u32string::const_iterator;
    using UStringIteratorPair = IteratorPair<UStringCIter>;
    struct Response {
        UStringCIter next;
        int token_type;
        bool always_hardwrap;
    };
    // token type's returned by the default instance
    static constexpr const int REGULAR_SEQUENCE   = 0;
    static constexpr const int LEADING_WHITESPACE = 1;
    /** @note Puesdo-writable (and stateless) object */
    static CodeModeler & default_instance();
    virtual ~CodeModeler();
    virtual void reset_state() = 0;
    virtual Response update_model(UStringCIter, Cursor) = 0;
};

class TextLineImage {
public:
    static constexpr const int NO_LINE_NUMBER  = -1;
    using UStringCIter = std::u32string::const_iterator;
    TextLineImage();
    TextLineImage(const TextLineImage &) = delete;
    TextLineImage(TextLineImage &&);

    TextLineImage & operator = (const TextLineImage &) = delete;
    TextLineImage & operator = (TextLineImage &&);

    ~TextLineImage() {}

    void update_modeler(CodeModeler &, const std::u32string &);
    void update_modeler(CodeModeler &, UStringCIter, UStringCIter);
    void clear_image();
    int height_in_cells() const;

    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_render_options(const RenderOptions &);

    void render_to(TargetTextGrid &, int offset) const;
    void swap(TextLineImage &);
    void copy_rendering_details(const TextLineImage & rhs);
    void constrain_to_width(int target_width);
    void set_line_number(int line_number);

    static void run_tests();
private:
    using UStrIteratorPair = IteratorPair<UStringCIter>;
    struct TokenInfo {
        TokenInfo(): type(0) {}
        TokenInfo(int type_, UStrIteratorPair pair_): type(type_), pair(pair_) {}
        TokenInfo(int type_, UStringCIter beg, UStringCIter end):
            type(type_), pair(beg, end)
        {}
        int type;
        UStrIteratorPair pair;
    };
    using TokenInfoCIter = std::vector<TokenInfo>::const_iterator;

    TokenInfoCIter render_row
        (TargetTextGrid & target, int offset, TokenInfoCIter word_itr,
         UStringCIter row_end) const;
    void fill_row_with_blanks(TargetTextGrid &, Cursor write_pos) const;
    void render_end_space(TargetTextGrid &, int offset) const;
    int handle_hard_wraps
        (const CodeModeler::Response &, UStringCIter, int working_width);
    void check_invarients() const;
    int m_grid_width;
    // required for edge case were all cells on the last row are occupied by
    // content, and therefore needing an extra space on the next so that the
    // user can type text at the end of the line
    int m_extra_end_space;
    // does not contain iterators begin and end in m_content
    std::vector<UStringCIter> m_row_ranges;

    const RenderOptions * m_rendering_options;

    int m_line_number;

    std::vector<TokenInfo> m_tokens;
};
