/****************************************************************************

    File: TargetTextGrid.cpp
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

#include "TargetTextGrid.hpp"

#include <stdexcept>

#include <cassert>

/* static */ const sf::Color RenderOptions::default_keyword_fore_c =
    sf::Color(200, 200, 0);
/* static */ const sf::Color RenderOptions::default_fore_c =
    sf::Color::White;
/* static */ const sf::Color RenderOptions::default_back_c =
    sf::Color(12, 12, 12);

RenderOptions::RenderOptions():
    m_tab_width(DEFAULT_TAB_WIDTH),
    m_fore_color(sf::Color::White),
    m_back_color(sf::Color(12, 12, 12)),
    m_keyword_color(sf::Color(200, 200, 0)),
    m_cursor_flash(false)
{}

void RenderOptions::add_keyword(const std::u32string & keyword) {
    // we do not care if the keyword is already present or not
    m_keywords.insert(keyword);
}

void RenderOptions::set_color_pair_option
    (decltype(DEFAULT_PAIR) choice, ColorPair pair)
{
    if (choice == DEFAULT_PAIR) {
        m_fore_color = pair.fore;
        m_back_color = pair.back;
    } else if (choice == KEYWORD_PAIR) {
        m_keyword_color = pair.fore;
    }
}

void RenderOptions::set_tab_width(int new_width) {
    if (new_width < 1) {
        throw std::invalid_argument("RenderOptions::set_tab_width: new tab "
                                    "width must be a positive integer");
    }
    m_tab_width = new_width;
}
ColorPair RenderOptions::get_pair_for_word
    (const std::u32string & word) const
{
    if (m_keywords.find(word) == m_keywords.end())
        return get_default_pair();
    else
        return ColorPair(m_keyword_color, m_back_color);
}

ColorPair RenderOptions::get_default_pair() const {
    return ColorPair(m_fore_color, m_back_color);
}

int RenderOptions::tab_width() const { return m_tab_width; }

void RenderOptions::set_text_selection(const UserTextSelection & sel)
    { m_user_text_selection = sel; }

void RenderOptions::set_cursor_flash_off()
    { m_cursor_flash = false; }

void RenderOptions::toggle_cursor_flash()
    { m_cursor_flash = !m_cursor_flash; }

RenderOptions::ColorPairTransformFunc
    RenderOptions::color_adjust_for(Cursor cursor) const
{
    if (m_user_text_selection.contains(cursor) ||
        (m_user_text_selection.end() == cursor && m_cursor_flash))
    {
        return invert;
    } else {
        return pass;
    }
}

/* static */ ColorPair RenderOptions::pass  (ColorPair color_pair)
    { return color_pair; }

/* static */ ColorPair RenderOptions::invert(ColorPair color_pair) {
    auto invert_single = [](sf::Color color)
        { return sf::Color(255 - color.r, 255 - color.g, 255 - color.r, color.a); };
    return ColorPair(invert_single(color_pair.fore),
                     invert_single(color_pair.back));
}

// ----------------------------------------------------------------------------

TargetTextGrid::~TargetTextGrid() {}

Cursor TargetTextGrid::next_cursor(Cursor cursor) const {
    if (!is_valid_cursor(cursor)) {
        throw std::invalid_argument("TargetTextGrid::next_cursor: given "
                                    "grid cursor is not valid.");
    }
    const auto width_ = width();
    if (++cursor.column == width_) {
        ++cursor.line;
        assert(cursor.line <= height());
        cursor.column = 0;
    }
    return cursor;
}

Cursor TargetTextGrid::end_cursor() const {
    return Cursor(height(), 0);
}

bool TargetTextGrid::is_valid_cursor(Cursor cursor) const noexcept {
    if (cursor.line < 0 || cursor.column < 0) return false;
    const auto end_c = end_cursor();
    if (end_c == cursor) return true;
    return cursor.line < end_c.line && cursor.column < width();
}

