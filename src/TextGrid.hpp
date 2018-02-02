/****************************************************************************

    File: TextGrid.hpp
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

#include "TextLines.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>

#include <ksg/Widget.hpp>
#include <ksg/DrawCharacter.hpp>

#include <common/DrawRectangle.hpp>

class TextGrid final : public ksg::Widget {
public:
    struct TargetInterface final : public TargetTextGrid {
        TargetInterface(TextGrid & parent): parent_grid(&parent) {}
        TargetInterface(const TargetInterface &) = default;
        ~TargetInterface() override;
        int width () const override { return parent_grid->width_in_cells(); }
        int height() const override { return parent_grid->height_in_cells(); }
        void set_cell(Cursor cursor, UChar uchr, ColorPair cpair) override
            { parent_grid->set_cell(cursor, cpair.fore, cpair.back, uchr); }
        TextGrid * parent_grid;
    };
    using StyleMap = ksg::StyleMap;

    static constexpr const char * const FONT = "text-grid-font";
    static constexpr const char * const CHARACTER_SIZE = "text-grid-char-size";

    static constexpr const int DEFAULT_CHAR_SIZE = 14;

    TextGrid();

    ~TextGrid() override;

    void process_event(const sf::Event &) override;

    /** in pixels */
    void set_location(float x, float y) override;

    /** in pixels */
    VectorF location() const override;

    /** in pixels */
    float width() const override;

    /** in pixels */
    float height() const override;

    void set_size_in_characters(int width, int height);

    int width_in_cells() const;
    int height_in_cells() const;

    void set_cell(Cursor, sf::Color fore, sf::Color back, UChar);
    void set_cell_fore_color(Cursor, sf::Color);
    void set_cell_back_color(Cursor, sf::Color);
    void set_cell_colors(Cursor, sf::Color fore, sf::Color back);
    void set_cell_character(Cursor, UChar);

    sf::Color cell_fore_color(Cursor) const;
    sf::Color cell_back_color(Cursor) const;

    void assign_font(const sf::Font &, int font_size = DEFAULT_CHAR_SIZE);

    void set_style(const StyleMap &) override;

    Cursor end_cursor() const;
    Cursor next_cursor(Cursor) const;

    TargetInterface as_target_interface()
        { return TargetInterface(*this); }

private:
    using DrawCharacter = ksg::without_advance::DrawCharacter;
    void draw(sf::RenderTarget & target, sf::RenderStates) const override;

    std::size_t cursor_to_cell(Cursor cur) const;

    void verify_cursor_validity(const char * caller, Cursor) const;

    struct TextCell {
        DrawCharacter character ;
        DrawRectangle background;
        UChar         identity  ;
    };
    std::vector<TextCell> m_cells;
    const sf::Font * m_font;
    VectorF m_location;
    float m_cell_width;
    float m_cell_height;
    int m_width;
    int m_char_size;
};
