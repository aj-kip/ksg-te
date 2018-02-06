/****************************************************************************

    File: UserTextSelection.hpp
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

#include <string>

#include "Cursor.hpp"

class TextLines;

class UserTextSelection {
public:
    // Cursors always in context of textlines (NOT the text grid)
    UserTextSelection(): m_alt_held(false) {}
    explicit UserTextSelection(Cursor starting_position);

    // events to move primary
    void move_left (const TextLines &);
    void move_right(const TextLines &);
    void move_down (const TextLines &);
    void move_up   (const TextLines &);
    void page_down (const TextLines &, int page_size);
    void page_up   (const TextLines &, int page_size);

    void push         (TextLines *, UChar);
    void delete_ahead (TextLines *);
    void delete_behind(TextLines *);

    // events controlling alt
    void hold_alt_cursor   () { m_alt_held = true ; }
    void release_alt_cursor() { m_alt_held = false; }
    bool alt_is_held() const  { return m_alt_held; }

    bool contains(Cursor) const;

    Cursor begin() const;
    Cursor end() const;

    bool operator == (const UserTextSelection &) const;
    bool operator != (const UserTextSelection &) const;

    static void run_tests();
private:
    void constrain_primary_update_alt(const TextLines &);
    bool primary_is_ahead() const;
    bool equal(const UserTextSelection &) const;
    void verify_text_lines_pointer(const char * caller, TextLines *) const;

    bool m_alt_held;
    Cursor m_primary;
    Cursor m_alt;
};
