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
