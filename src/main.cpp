/****************************************************************************

    File: main.cpp
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

#include <cstdint>
#include <cassert>

#include <vector>
#include <set>

#include <ksg/Widget.hpp>
#include <ksg/Frame.hpp>

#include <cassert>

#include "TextLines.hpp"
#include "TextGrid.hpp"
#include "UserTextSelection.hpp"

void handle_event(TextLines *, const sf::Event &);
void handle_event(UserTextSelection *, TextLines &tlines, const sf::Event &);

class EditorDialog final : public ksg::Frame {
public:
    EditorDialog(): m_delay(false) {}
    ~EditorDialog() override;
    void setup_dialog(const sf::Font &);
    void process_event(const sf::Event &) override;
    void do_update(float et);
private:
    TextLines m_lines;

    TextGrid m_grid;
    float m_delay;
    Cursor m_cursor;
    UserTextSelection m_user_selection;
    RenderOptions m_render_options;
};

int main() {
#   ifndef NDEBUG
    TextLine::run_tests();
    TextLines::run_tests();
    //UserTextSelection::run_tests();
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
    window.setFramerateLimit(60);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            editor.process_event(event);
            if (event.type == sf::Event::Closed)
                window.close();
        }

        editor.do_update(clock.getElapsedTime().asSeconds());
        clock.restart();
        window.clear();
        window.draw(editor);
        window.display();
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
         "end\n"
         "This is a very long line which contains over eighty characters, which is made evident by this very verbose sentence.\n"
         "abcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrstabcdefghijklmnopqrst\n"
         "last line.";

    m_lines.set_content(sample_code);
    m_grid.set_size_in_characters(80, 30);
    m_render_options.add_keyword(U"function");
    m_render_options.add_keyword(U"local");
    m_render_options.add_keyword(U"return");
    m_render_options.add_keyword(U"end");
    m_lines.constrain_to_width(m_grid.width_in_cells());
    m_lines.assign_render_options(m_render_options);

    m_grid.assign_font(font);
    m_render_options.set_text_selection(m_user_selection);

    m_lines.render_to(m_grid.as_target_interface(), 0);

    add_widget(&m_grid);
    set_style(styles);
    update_geometry();
}

void EditorDialog::process_event(const sf::Event & event) {
    Frame::process_event(event);
    auto old_selection = m_user_selection;
    handle_event(&m_user_selection, m_lines, event);
    if (old_selection != m_user_selection) {
        m_render_options.set_text_selection(m_user_selection);
        m_render_options.toggle_cursor_flash();
        m_lines.render_to(m_grid.as_target_interface(), 0);
    }
}

void EditorDialog::do_update(float et) {
    m_delay += et;
    if (m_delay > 0.3f) {
        m_delay = 0.f;
        m_render_options.toggle_cursor_flash();
        m_lines.render_to(m_grid.as_target_interface(), 0);
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

void handle_event
    (UserTextSelection * selection, TextLines & tlines,
     const sf::Event & event)
{
    assert(selection);
    switch (event.type) {
    case sf::Event::KeyReleased:
        if (event.key.shift) {
            selection->hold_alt_cursor();
        } else {
            selection->release_alt_cursor();
        }
        switch (event.key.code) {
        case sf::Keyboard::Down:
            selection->move_down(tlines);
            break;
        case sf::Keyboard::Up:
            selection->move_up(tlines);
            break;
        case sf::Keyboard::Left:
            selection->move_left(tlines);
            break;
        case sf::Keyboard::Right:
            selection->move_right(tlines);
            break;
        case sf::Keyboard::Delete:
            selection->delete_ahead(&tlines);
            break;
        case sf::Keyboard::BackSpace:
            selection->delete_behind(&tlines);
            break;
        case sf::Keyboard::Return:
            selection->push(&tlines, TextLines::NEW_LINE);
            break;
        default:break;
        }
        break;
    case sf::Event::KeyPressed:
        break;
    case sf::Event::TextEntered:
        if (event.text.unicode == 8 || event.text.unicode == 127 || event.text.unicode == 13) break;
        selection->push(&tlines, UChar(event.text.unicode));
        break;
    default: break;
    }
}
