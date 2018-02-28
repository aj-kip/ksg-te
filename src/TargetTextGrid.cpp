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
#include "LuaCodeModeler.hpp"

#include <stdexcept>

#include <cassert>

/* static */ const sf::Color RenderOptions::default_keyword_fore_c =
    sf::Color(200, 200, 0);
/* static */ const sf::Color RenderOptions::default_fore_c =
    sf::Color::White;
/* static */ const sf::Color RenderOptions::default_back_c =
    sf::Color(12, 12, 12);

NullTextGrid::NullTextGrid(): m_width(1), m_height(1) {}

void NullTextGrid::set_cell(Cursor cursor, UChar, ColorPair) {
    if (is_valid_cursor(cursor)) return;
    throw std::invalid_argument
        ("NullTextGrid::set_cell: attempted to write to an invalid grid position.");
}

void NullTextGrid::verify_dim(const char * caller, int dim) const {
    if (dim > 0) return;
    throw std::invalid_argument(std::string(caller) +
                                ": dimension must be at least one.");
}

/* static */ const RenderOptions & RenderOptions::get_default_instance() {
    static RenderOptions instance;
    return instance;
}

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

ColorPair RenderOptions::get_pair_for_token_type(int tid) const {
    return LuaCodeModeler::colors_for_pair(tid);
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

SubTextGrid TargetTextGrid::make_sub_grid
    (Cursor cursor, int width, int height)
{ return SubTextGrid(this, cursor, width, height); }

// ----------------------------------------------------------------------------

namespace {

TargetTextGrid * verify_parent(TargetTextGrid * ptr);

Cursor verify_cursor(Cursor cursor, TargetTextGrid * parent);

int verify_valid_dim(int dim, int max, const char * dim_name);

} // end of <anonymous> namespace

SubTextGrid::SubTextGrid():
    m_parent(nullptr),
    m_width (REST_OF_GRID),
    m_height(REST_OF_GRID)
{}

// members are initialized in order of appearence in source
SubTextGrid::SubTextGrid
    (TargetTextGrid * parent, Cursor cursor, int width, int height):
    m_parent(verify_parent(parent)),
    m_offset(verify_cursor(cursor, parent)),
    m_width (verify_valid_dim(width , parent->width (), "width" )),
    m_height(verify_valid_dim(height, parent->height(), "height"))
{
    if (width  == REST_OF_GRID) m_width  = parent->width () - m_offset.column;
    if (height == REST_OF_GRID) m_height = parent->height() - m_offset.line  ;
}

int SubTextGrid::width () const { return m_width ; }

int SubTextGrid::height() const { return m_height; }

void SubTextGrid::set_cell(Cursor cursor, UChar uchr, ColorPair pair) {
    // lol how?!
    if (m_parent) {
        Cursor adjusted(cursor.line + m_offset.line, cursor.column + m_offset.column);
        m_parent->set_cell(adjusted, uchr, pair);
        return;
    }
    throw std::invalid_argument("SubTextGrid::set_cell: parent pointer "
                                "must point to a target text grid.");

}

namespace {

TargetTextGrid * verify_parent(TargetTextGrid * ptr) {
    if (ptr) return ptr;
    throw std::invalid_argument("SubTextGrid::SubTextGrid: parent pointer "
                                "must point to a target text grid.");
}

Cursor verify_cursor(Cursor cursor, TargetTextGrid * parent) {
    assert(parent);
    if (parent->is_valid_cursor(cursor)) return cursor;
    throw std::invalid_argument("SubTextGrid::SubTextGrid: offset cursor "
                                "must be inside the parent grid.");
}

int verify_valid_dim(int dim, int max, const char * dim_name) {
    if (dim == SubTextGrid::REST_OF_GRID) return dim;
    if (dim > 0) return dim;
    if (dim > max) {
        throw std::invalid_argument
            ("SubTextGrid::SubTextGrid: " + std::string(dim_name) + " "
             "may not exceed the " + std::string(dim_name) + " of the parent.");
    }
    assert(dim < 1);
    throw std::invalid_argument
        ("SubTextGrid::SubTextGrid: " + std::string(dim_name) + " "
         "must be a positive integer or REST_OF_GRID sentinel.");
}

} // end of <anonymous> namespace
