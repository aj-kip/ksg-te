/****************************************************************************

    File: Cursor.hpp
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

#include <cstddef>

using UChar = char32_t;

struct Cursor {
    Cursor(): line(0), column(0) {}

    Cursor(int line_, int column_): line(line_), column(column_) {}

    bool operator == (const Cursor & lhs) const
        { return line == lhs.line && column == lhs.column; }

    bool operator != (const Cursor & lhs) const
        { return line != lhs.line || column != lhs.column; }

    int line;
    int column;
};

struct CursorHasher {
    std::size_t operator () (const Cursor & rhs) const
        { return std::size_t(rhs.line*3753 ^ rhs.column); }
};

