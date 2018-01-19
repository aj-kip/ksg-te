#include <cstdint>
#include <cassert>

#include <vector>
#include <set>

#include <ksg/Widget.hpp>
#include <ksg/Frame.hpp>

#include <cassert>

#include "TextLines.hpp"
#include "TextRenderer.hpp"
#include "TextGrid.hpp"

void handle_event(TextLines *, const sf::Event &);

class EditorDialog final : public ksg::Frame {
public:
    EditorDialog(): m_delay(false) {}
    ~EditorDialog() override;
    void setup_dialog(const sf::Font &);
    void do_update(float et);
private:
    TextLines m_lines;
    TextRenderer m_highligher;
    TextGrid m_grid;
    float m_delay;
    Cursor m_cursor;
};

int main() {
#   ifndef NDEBUG
    TextLines::run_test();
    TextRenderer::run_tests();
#   endif
    EditorDialog editor;
    sf::Font font;
    if (!font.loadFromFile("SourceCodePro-Regular.ttf")) {
        throw std::runtime_error("Cannot load font");
    }
    editor.setup_dialog(font);

    auto editor_width  = unsigned(editor.width ());
    auto editor_height = unsigned(editor.height());
    sf::RenderWindow window(sf::VideoMode(editor_width, editor_height),
        "Window Title");
    sf::Clock clock;
    window.setFramerateLimit(20);
    bool has_events = true;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            has_events = true;
            editor.process_event(event);
            if (event.type == sf::Event::Closed)
                window.close();
        }
        //if (editor.requesting_to_close())
            //window.close();
        editor.do_update(clock.getElapsedTime().asSeconds());
        clock.restart();
#       if 0
        if (has_events) {
#       endif
            window.clear();
            window.draw(editor);
            window.display();

#       if 0
            has_events = false;
        } else {
            sf::sleep(sf::microseconds(16667));
        }
#       endif
    }
    return 0;
}

EditorDialog::~EditorDialog() {}

void EditorDialog::setup_dialog(const sf::Font & font) {
    auto styles = ksg::construct_system_styles();
    styles[ksg::Frame::GLOBAL_FONT] = ksg::StylesField(&font);
    styles[TextGrid::FONT] = ksg::StylesField(&font);
    std::u32string sample_code =
        U"function do_something(a, b)\n"
         "    local c = pull(a)\n"
         "    c[1] = c[1] + a*b\n"
         "    return c\n"
         "end";
    Cursor cursor;
    for (auto uchr : sample_code)
        cursor = m_lines.push(cursor, uchr);
    m_grid.set_size_in_characters(80, 30);
    m_highligher.add_keyword(U"function");
    m_highligher.add_keyword(U"local");
    m_highligher.add_keyword(U"return");
    m_highligher.add_keyword(U"end");
    m_highligher.setup_defaults();

    m_highligher.render_lines_to(&m_grid, Cursor(0, 0), m_lines);
    m_grid.assign_font(font);
    add_widget(&m_grid);
    set_style(styles);
    update_geometry();
}

void EditorDialog::do_update(float et) {
    m_delay += et;
    if (m_delay > 0.5f) {
        m_delay = 0.f;
        auto invert = TextRenderer::invert;
        m_grid.set_cell_colors(m_cursor,
            invert(m_grid.cell_fore_color(m_cursor)),
            invert(m_grid.cell_back_color(m_cursor)));
    }
}

void handle_event(TextLines * text, const sf::Event & event) {
    assert(text);
    switch (event.type) {
    case sf::Event::TextEntered:
        //text->push( event.text.unicode;
    case sf::Event::KeyReleased:
    case sf::Event::KeyPressed:
        break;
    default: break;
    }
    if (event.type != sf::Event::TextEntered) return;
}
