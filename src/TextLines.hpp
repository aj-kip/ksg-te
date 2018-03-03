/****************************************************************************

    File: TextLines.hpp
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
#include "TextLine.hpp"

#pragma once

class TextLines {
public:
    static constexpr const UChar NEW_LINE = U'\n';
    using UStringCIter = std::u32string::const_iterator;

    TextLines();
    explicit TextLines(const std::u32string &);

    // ------------------------ whole content editing -------------------------

    void constrain_to_width(int);
    void set_content(const std::u32string &);
    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_render_options(const RenderOptions &);
    void assign_default_render_options();

    void update_modeler(CodeModeler &);

    // ----------------------- single character editing -----------------------

    Cursor push(Cursor, UChar);
    // user presses "del"
    Cursor delete_ahead(Cursor);
    // user presses "backspace"
    Cursor delete_behind(Cursor);
    Cursor wipe(Cursor beg, Cursor end);
    std::u32string copy_characters_from(Cursor beg, Cursor end) const;
    void deposit_chatacters_to
        (UStringCIter beg, UStringCIter end, Cursor pos = Cursor());
    Cursor deposit_chatacters_to
        (const UChar * beg, const UChar * end, Cursor pos = Cursor());

    // ------------------------------ accessors -------------------------------

    Cursor next_cursor(Cursor) const;
    Cursor previous_cursor(Cursor) const;
    Cursor constrain_cursor(Cursor) const noexcept;

    /** The end cursor carries the same meaning as end iterators do for STL
     *  containers, essentially "one past the end". What this means for the
     *  actual value of the cursor is that is starts on one past the last line
     *  of this collection.
     *  @return
     */
    Cursor end_cursor() const;
    bool is_valid_cursor(Cursor) const noexcept;

    void render_to(TargetTextGrid &, int offset) const;
    void render_to(TargetTextGrid && rvalue, int offset) const
        { render_to(rvalue, offset); }

    const std::vector<TextLine> & lines() const
        { return m_lines; }

    static void run_tests();
private:
    /** @param func must take signature:
     *              void(*)(int line_num, int line_begin, int line_end)
     */
    template <typename Func>
    void for_each_line_in_range(Cursor beg, Cursor end, Func && func) const;
    void check_invarients() const;
    void verify_cursor_validity(const char * caller, Cursor) const;

    // A should be called after every modification of the m_lines vector
    //

    class LinesCollection {
    public:
        TextLine & push_new_line();
        const std::vector<TextLine> & text_lines();
    };

    void refresh_lines_information();
    std::vector<TextLine> m_lines;
    const RenderOptions * m_rendering_options;
    int m_width_constraint;
};

