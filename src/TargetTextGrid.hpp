/****************************************************************************

    File: TargetTextGrid.hpp
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

#include "Cursor.hpp"
#include "UserTextSelection.hpp"

#include <string>
#include <set>
#include <SFML/Graphics/Color.hpp>

struct ColorPair {
    ColorPair(){}
    ColorPair(sf::Color fore_, sf::Color back_): fore(fore_), back(back_) {}
    sf::Color fore;
    sf::Color back;
};

template <sf::Color(*transform)(sf::Color)>
inline ColorPair apply_to
    (const ColorPair & pair)
    { return ColorPair(transform(pair.fore), transform(pair.back)); }

class TargetTextGrid {
public:
    virtual ~TargetTextGrid();
    virtual int width () const = 0;
    virtual int height() const = 0;
    virtual void set_cell(Cursor, UChar, ColorPair) = 0;
    Cursor next_cursor(Cursor) const;
    Cursor end_cursor() const;
    bool is_valid_cursor(Cursor) const noexcept;
};

class RenderOptions {
public:
    static const sf::Color default_keyword_fore_c;
    static const sf::Color default_fore_c;
    static const sf::Color default_back_c;
    static constexpr const int DEFAULT_TAB_WIDTH = 4;
    using ColorPairTransformFunc = ColorPair (*)(ColorPair);
    enum { DEFAULT_PAIR, KEYWORD_PAIR };
    RenderOptions();
    void add_keyword(const std::u32string &);
    void set_color_pair_option(decltype(DEFAULT_PAIR), ColorPair);
    void set_tab_width(int);
    ColorPair get_pair_for_word(const std::u32string &) const;
    ColorPair get_default_pair() const;
    int tab_width() const;

    void set_text_selection(const UserTextSelection &);
    //const UserTextSelection & text_selection() const;

    void set_cursor_flash_off();
    void toggle_cursor_flash();
    ColorPairTransformFunc color_adjust_for(Cursor) const;

    static ColorPair pass  (ColorPair);
    static ColorPair invert(ColorPair);
private:
    int m_tab_width;
    std::set<std::u32string> m_keywords;
    sf::Color m_fore_color, m_back_color, m_keyword_color;
    UserTextSelection m_user_text_selection;
    bool m_cursor_flash;
};
