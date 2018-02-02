/****************************************************************************

    File: UserTextSelection.cpp
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

#include "UserTextSelection.hpp"
#include "TextLines.hpp"

#include <stdexcept>

#include <cassert>

UserTextSelection::UserTextSelection(Cursor starting_position):
    m_alt_held(false            ),
    m_primary (starting_position),
    m_alt     (starting_position)
{}

void UserTextSelection::move_left(const TextLines & tlines) {
    if (m_primary == Cursor()) return;
    m_primary = tlines.constrain_cursor(tlines.previous_cursor(m_primary));
    if (!m_alt_held)
        m_alt = m_primary;
}

void UserTextSelection::move_right(const TextLines & tlines) {
    if (m_primary == tlines.end_cursor()) return;
    m_primary = tlines.constrain_cursor(tlines.next_cursor(m_primary));
    if (!m_alt_held)
        m_alt = m_primary;
}

void UserTextSelection::move_down(const TextLines & tlines) {
    ++m_primary.line;
    m_primary = tlines.constrain_cursor(m_primary);
    constrain_primary_update_alt(tlines);
}

void UserTextSelection::move_up(const TextLines & tlines) {
    --m_primary.line;
    m_primary = tlines.constrain_cursor(m_primary);
    constrain_primary_update_alt(tlines);
}

void UserTextSelection::page_down
    (const TextLines & tlines, int page_size)
{
    m_primary.line += page_size;
    constrain_primary_update_alt(tlines);
}

void UserTextSelection::page_up
    (const TextLines & tlines, int page_size)
{
    m_primary.line -= page_size;
    constrain_primary_update_alt(tlines);
}

bool UserTextSelection::contains(Cursor cursor) const {
    auto end_ = end(), beg = begin();
    return cursor.line >= beg.line    && cursor.column >= beg.column &&
           cursor.line <  end_.column && cursor.column <  end_.column;
}

UserTextSelection::StringSelectionIters UserTextSelection::
    ranges_for_string(Cursor cursor, const std::u32string & buff,
                      const TextLines & tlines) const
{
    auto itr_beg = buff.begin(), itr_highlight_beg = buff.begin();
    const auto beg_cursor = begin();
    while (cursor != beg_cursor && itr_highlight_beg != buff.end()) {
        ++itr_highlight_beg;
        cursor = tlines.next_cursor(cursor);
    }
    if (itr_highlight_beg == buff.end()) {
        return StringSelectionIters(itr_beg, buff.end(), buff.end(), buff.end());
    }
    const auto end_cursor = end();
    auto itr_highlight_end = itr_highlight_beg;
    while (cursor != end_cursor && itr_highlight_end != buff.end()) {
        ++itr_highlight_end;
        cursor = tlines.next_cursor(cursor);
    }
    return StringSelectionIters
        (itr_beg, itr_highlight_beg, itr_highlight_end, buff.end());
}

Cursor UserTextSelection::begin() const {
    return primary_is_ahead() ? m_alt : m_primary;
}

Cursor UserTextSelection::end() const {
    return primary_is_ahead() ? m_primary : m_alt;
}

bool UserTextSelection::operator == (const UserTextSelection & rhs) const
    { return equal(rhs); }

bool UserTextSelection::operator != (const UserTextSelection & rhs) const
    { return !equal(rhs); }

/* static */ void UserTextSelection::run_tests() {
    const TextLines test_tlines_c = TextLines(
        U"sample text\n"
         "second line with five words\n"
         "and then finally the third line\n"
         "shortest");
    // 1. move right
    {
    UserTextSelection uts(Cursor(0, 3));
    for (int i = 0; i != 3; ++i)
        uts.move_right(test_tlines_c);
    assert(uts.begin() == uts.end());
    assert(uts.begin() == Cursor(0, 6));
    }
    // 2. move left
    {
    UserTextSelection uts(Cursor(1, 7));
    for (int i = 0; i != 3; ++i)
        uts.move_left(test_tlines_c);
    assert(uts.begin() == uts.end());
    assert(uts.begin() == Cursor(1, 4));
    }
    // 3. move up
    {
    UserTextSelection uts(Cursor(1, 5));
    uts.move_up(test_tlines_c);
    assert(uts.begin() == uts.end());
    assert(uts.begin() == Cursor(0, 5));
    }
    // 4. move down
    {
    UserTextSelection uts(Cursor(1, 5));
    uts.move_down(test_tlines_c);
    assert(uts.begin() == Cursor(2, 5));
    }
    // 5. move left -> previous line
    {
    UserTextSelection uts(Cursor(1, 3));
    for (int i = 0; i != 4; ++i)
        uts.move_left(test_tlines_c);
    assert(uts.begin() == Cursor(0, 11));
    }
    // 6. move right -> next line
    {
    UserTextSelection uts(Cursor(0, 7));
    for (int i = 0; i != 5; ++i)
       uts.move_right(test_tlines_c);
    assert(uts.begin() == Cursor(1, 0));
    }
    // 7. move up -> constrain
    {
    UserTextSelection uts(Cursor(1, 16));
    assert(test_tlines_c.is_valid_cursor(uts.begin()));
    uts.move_up(test_tlines_c);
    assert(uts.begin() == Cursor(0, 11));
    }
    // 8. move down -> constrain
    {
    UserTextSelection uts(Cursor(2, 15));
    assert(test_tlines_c.is_valid_cursor(uts.begin()));
    uts.move_down(test_tlines_c);
    assert(uts.begin() == Cursor(3, 8));
    }
    // 9. hold alt -> move right
    {
    UserTextSelection uts(Cursor(0, 3));
    uts.hold_alt_cursor();
    uts.move_right(test_tlines_c);
    assert(uts.begin() == Cursor(0, 3) && uts.end() == Cursor(0, 4));
    }
    // 10. hold alt -> move left
    {
    UserTextSelection uts(Cursor(0, 9));
    uts.hold_alt_cursor();
    uts.move_left(test_tlines_c);
    assert(uts.begin() == Cursor(0, 8) && uts.end() == Cursor(0, 9));
    }
    // 11. hold alt + move right -> next line
    {
    UserTextSelection uts(Cursor(0, 9));
    uts.hold_alt_cursor();
    for (int i = 0; i != 4; ++i)
        uts.move_right(test_tlines_c);
    assert(uts.end  () == Cursor(1, 1));
    assert(uts.begin() == Cursor(0, 9));
    }
    // 12. hold alt + move left -> previous line
}

/* private */ void UserTextSelection::constrain_primary_update_alt
    (const TextLines & tlines)
{
    m_primary = tlines.constrain_cursor(m_primary);
    if (!m_alt_held)
        m_alt = m_primary;
}

/* private */ bool UserTextSelection::primary_is_ahead() const {
    if (m_primary.line > m_alt.line) return true;
    return m_primary.column > m_alt.column;
}

/* private */ bool UserTextSelection::equal
    (const UserTextSelection & rhs) const
{
    return rhs.m_alt == m_alt && rhs.m_alt_held == m_alt_held &&
           rhs.m_primary == m_primary;
}

// ----------------------------------------------------------------------------

UserTextSelection::StringSelectionIters::StringSelectionIters() {}

UserTextSelection::StringSelectionIters::StringSelectionIters
    (Iter beg_, Iter highlight_beg_, Iter highlight_end_, Iter end_):
    m_begin(beg_),
    m_highlight_beg(highlight_beg_),
    m_highlight_end(highlight_end_),
    m_end(end_)
{
    if (!iterators_are_good()) {
        throw std::invalid_argument(
            "UserTextSelection::StringSelectionIters::StringSelectionIters: "
            "for each iterator from left to right, the right iterator must "
            "be equal or greater than the left.");
    }
}

/* private */ bool UserTextSelection::StringSelectionIters::
    iterators_are_good() const
{
    return m_begin <= m_highlight_beg && m_highlight_beg <= m_highlight_end &&
           m_highlight_end <= m_end;
}

