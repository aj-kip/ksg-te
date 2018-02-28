/****************************************************************************

    File: TextGrid.cpp
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

#include "KsgTextGrid.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cassert>

KsgTextGrid::TargetInterface::~TargetInterface() {}

KsgTextGrid::KsgTextGrid():
    m_font(nullptr),
    m_cell_width(0.f),
    m_cell_height(0.f),
    m_width(0),
    m_char_size(0)
{}

KsgTextGrid::~KsgTextGrid() {}

void KsgTextGrid::process_event(const sf::Event &) {}

void KsgTextGrid::set_location(float x, float y) {
    m_location = VectorF(x, y);
    for (auto cur = Cursor(0, 0); cur != end_cursor(); cur = next_cursor(cur)) {
        auto & cell = m_cells[cursor_to_cell(cur)];
        VectorF new_location(cur.column*m_cell_width, cur.line*m_cell_height);
        new_location += m_location;
        const auto & glyph = m_font->getGlyph(cell.identity, unsigned(m_char_size), false);
        cell.character = DrawCharacter(glyph, cell.character.color());
        cell.character .move        (new_location.x, new_location.y + m_cell_height*0.8f);
        cell.background.set_position(new_location.x, new_location.y);
    }
}

KsgTextGrid::VectorF KsgTextGrid::location() const { return m_location; }

float KsgTextGrid::width() const { return m_cell_width*float(m_width); }

float KsgTextGrid::height() const { return m_cell_height*float(height_in_cells()); }

void KsgTextGrid::set_size_in_characters(int width, int height) {
    m_width = width;
    m_cells.resize(std::size_t(width*height));
}

int KsgTextGrid::width_in_cells() const { return m_width; }

int KsgTextGrid::height_in_cells() const
    { return int(m_cells.size()) / m_width; }

void KsgTextGrid::set_cell
    (Cursor cursor, sf::Color fore, sf::Color back, UChar uchr)
{
    verify_cursor_validity("TextGrid::set_cell", cursor);
    auto & cell = m_cells[cursor_to_cell(cursor)];
    cell.identity = uchr;
    cell.background.set_color(back);
    if (!m_font) {
        cell.character.set_color(fore);
        return;
    }
    const auto & glyph = m_font->getGlyph(uchr, unsigned(m_char_size), false);    
    cell.character = DrawCharacter(glyph, fore);
    cell.character.move(cursor.column*m_cell_width  + m_location.x,
                        cursor.line  *m_cell_height + m_location.y + m_cell_height*0.8f);
}

void KsgTextGrid::set_cell_fore_color(Cursor cur, sf::Color color) {
    verify_cursor_validity("TextGrid::set_cell_fore_color", cur);
    m_cells[cursor_to_cell(cur)].character.set_color(color);
}

void KsgTextGrid::set_cell_back_color(Cursor cursor, sf::Color color) {
    verify_cursor_validity("TextGrid::set_cell_back_color", cursor);
    m_cells[cursor_to_cell(cursor)].background.set_color(color);
}

void KsgTextGrid::set_cell_colors(Cursor cursor, sf::Color fore, sf::Color back) {
    verify_cursor_validity("TextGrid::set_cell_colors", cursor);
    m_cells[cursor_to_cell(cursor)].background.set_color(back);
    m_cells[cursor_to_cell(cursor)].character .set_color(fore);
}

void KsgTextGrid::set_cell_character(Cursor cursor, UChar identity) {
    verify_cursor_validity("TextGrid::set_cell_character", cursor);
    m_cells[cursor_to_cell(cursor)].identity = identity;
}

sf::Color KsgTextGrid::cell_fore_color(Cursor cursor) const {
    verify_cursor_validity("TextGrid::cell_fore_color", cursor);
    return m_cells[cursor_to_cell(cursor)].character.color();
}

sf::Color KsgTextGrid::cell_back_color(Cursor cursor) const {
    verify_cursor_validity("TextGrid::cell_back_color", cursor);
    return m_cells[cursor_to_cell(cursor)].background.color();
}

void KsgTextGrid::assign_font(const sf::Font & font, int font_size) {
    m_font = &font;
    m_char_size = font_size;
    m_cell_height = m_font->getLineSpacing(unsigned(font_size));
    const auto & glyph = m_font->getGlyph(U'a', unsigned(font_size), false);
    m_cell_width = glyph.bounds.width + glyph.advance*0.5f;
    // update cells
    for (Cursor cur; cur != end_cursor(); cur = next_cursor(cur)) {
        auto & cell = m_cells[cursor_to_cell(cur)];
        const auto & glyph = m_font->getGlyph(cell.identity, unsigned(m_char_size), false);
        cell.character = DrawCharacter(glyph, cell.character.color());
        cell.background.set_size(m_cell_width, m_cell_height);
    }
}

void KsgTextGrid::set_style(const StyleMap & styles) {
    int font_size = DEFAULT_CHAR_SIZE;
    auto itr = styles.find(CHARACTER_SIZE);
    if (itr != styles.end()) {
        if (itr->second.is_type<float>()) {
            font_size = int(itr->second.as<float>());
        }
    }
    itr = styles.find(FONT);
    if (itr != styles.end()) {
        assign_font(*itr->second.as<const sf::Font *>(), font_size);
    }
}

Cursor KsgTextGrid::end_cursor() const {
    return Cursor(height_in_cells(), 0);
}
Cursor KsgTextGrid::next_cursor(Cursor cur) const {
    if (++cur.column == width_in_cells()) {
        cur.column = 0;
        ++cur.line;
    }
    return cur;
}

/* private */ void KsgTextGrid::draw
    (sf::RenderTarget & target, sf::RenderStates states) const
{
    states.texture = &m_font->getTexture(unsigned(m_char_size));
    for (const auto & cell : m_cells) {
        target.draw(cell.background);
        target.draw(cell.character, states);
    }
}

/* private */ std::size_t KsgTextGrid::cursor_to_cell(Cursor cur) const {
    assert(!(cur.column >= m_width) && !(cur.line >= height_in_cells()));
    return std::size_t(cur.column + cur.line*m_width);
}

/* private */ void KsgTextGrid::verify_cursor_validity
    (const char * caller, Cursor cur) const
{
    if (!(cur.column >= m_width) && !(cur.line >= height_in_cells()) &&
        !(cur.column < 0       ) && !(cur.line < 0                )) return;
    throw std::out_of_range(std::string(caller) + ": cursor is out of range.");
}
