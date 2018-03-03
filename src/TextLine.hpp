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

#include "Cursor.hpp"
#include "TargetTextGrid.hpp"
#include "IteratorPair.hpp"
#include "TextLineImage.hpp"

#include <common/MultiType.hpp>

#include <vector>
#include <string>
#include <stdexcept>
#include <memory>

#pragma once

// content string + extra ending space
class TextLine {
public:
    enum ContentTakingPlacement { PLACE_AT_BEGINING, PLACE_AT_END };
    static constexpr const int MERGE_REQUESTED = -1;
    static constexpr const int SPLIT_REQUESTED = -1;
    static constexpr const int NO_LINE_NUMBER  = -1;
    using UStringCIter = std::u32string::const_iterator;
    using UStrIteratorPair = IteratorPair<UStringCIter>;
    TextLine();
    TextLine(const TextLine &);
    TextLine(TextLine &&);
    explicit TextLine(const std::u32string &);

    TextLine & operator = (const TextLine &);
    TextLine & operator = (TextLine &&);

    // -------------------------- TextLine Settings ---------------------------

    void constrain_to_width(int);

    void set_content(const std::u32string &);
    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_render_options(const RenderOptions &);

    void set_line_number(int);

    void assign_default_render_options();

    // ------------------------ whole content editing -------------------------

    TextLine split(int column);

    void take_contents_of(TextLine &, decltype(PLACE_AT_BEGINING));

    int wipe(int beg, int end);

    void copy_characters_from(std::u32string &, int beg, int end) const;

    int deposit_chatacters_to
        (UStringCIter beg, UStringCIter end, int pos);

    int deposit_chatacters_to(const UChar * beg, const UChar * end, int pos);

    void swap(TextLine &);

    void update_modeler(CodeModeler &);

    // ----------------------- single character editing -----------------------

    // may return SPLIT_REQUESTED
    int push(int column, UChar);
    // may return MERGE_REQUESTED
    int delete_ahead(int column);
    // may return MERGE_REQUESTED
    int delete_behind(int column);

    // ------------------------------ accessors -------------------------------

    int height_in_cells() const;
    const std::u32string & content() const;
    int content_length() const { return int(content().length()); }

    void render_to(TargetTextGrid &, int offset) const;

    static void run_tests();
private:
    void verify_column_number(const char * callername, int) const;
    void verify_text(const char * callername, UChar) const;
    void verify_text(const char * callername, const UChar *, const UChar *) const;
    std::u32string m_content;
    TextLineImage m_image;
};
