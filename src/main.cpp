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
#include <fstream>

#include <ksg/Widget.hpp>
#include <ksg/Frame.hpp>

#include <cassert>

#include "TextLine.hpp"
#include "TextLines.hpp"
#include "KsgTextGrid.hpp"
#include "UserTextSelection.hpp"

constexpr const auto * const SAMPLE_CODE =
    U"function do_something(a, b)\n"
     "    local c = pull(a)\n"
     "    c[1] = c[1] + a*b\n"
     "    return c\n"
     "end\n"
     "local function make_vector()\n"
     "    local self = { x = 0, y = 0 }\n"
     "    -- contextual highlight? make magnitude a function's color?\n"
     "    -- to do this automatically, I would need something that parses Lua\n"
     "    self.magnitude = function()\n"
     "        return math.sqrt(self.x*self.x + self.y*self.y)\n"
     "    end\n"
     "    return self\n"
     "end";

void handle_event(TextLines *, const sf::Event &);
void handle_event(UserTextSelection *, TextLines * tlines, const sf::Event &);
std::u32string load_ascii_textfile(const char * filename);
int bottom_offset(const TextLines &, const TargetTextGrid &);
class TextTyperBot;

std::u32string expand_char_width(const std::string & str) {
    std::u32string rv;
    for (auto c : str) rv += UChar(c);
    return rv;
}

class EditorDialog final : public ksg::Frame {
public:
    EditorDialog(): m_delay(false) {}
    ~EditorDialog() override;
    void setup_dialog(const sf::Font &);
    void process_event(const sf::Event &) override;
    void do_update(float et, TextTyperBot &);
private:
    TextLines m_lines;

    KsgTextGrid m_grid;
    std::unique_ptr<KsgTextGrid::TargetInterface> m_interface;
    SubTextGrid m_elapsed_time_grid;
    SubTextGrid m_doc;
    float m_delay;
    Cursor m_cursor;
    UserTextSelection m_user_selection;
    RenderOptions m_render_options;
};

class TextTyperBot {
public:
    enum { HAS_UPDATE, NO_UPDATE };
    TextTyperBot();
    decltype (NO_UPDATE) update(TextLines &, UserTextSelection &, double et);
    TextTyperBot & set_content(const std::u32string &);
    // chars per second
    TextTyperBot & set_type_rate(double);
private:
    std::u32string m_content;
    std::size_t m_current_index;
    double m_type_rate;
    double m_delay;
    Cursor m_curent_cursor;
};

int main() {
#   if 0//ndef NDEBUG
    TextLine::run_tests();
    TextLines::run_tests();
    UserTextSelection::run_tests();
#   endif
    {
    TextLine tline;
    tline.push(0, U'[');
    }
    EditorDialog editor;
    TextTyperBot bot;
    (void)bot.set_content(load_ascii_textfile("vector.lua")).set_type_rate(0.0075);

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

        editor.do_update(clock.getElapsedTime().asSeconds(), bot);
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
    styles[KsgTextGrid::FONT] = ksg::StylesField(&font);

    m_grid.set_size_in_characters(80, 30);
    auto lua_keywords = {
        U"and"   ,    U"break" ,    U"do"  ,      U"else"    ,  U"elseif",
        U"end"   ,    U"false" ,    U"for" ,      U"function",  U"if"    ,
        U"in"    ,    U"local" ,    U"nil" ,      U"not"     ,  U"or"    ,
        U"repeat",    U"return",    U"then",      U"true"    ,  U"until" ,
        U"while"
    };
    for (auto keyword : lua_keywords)
        m_render_options.add_keyword(keyword);
    m_lines.constrain_to_width(m_grid.width_in_cells());
    m_lines.assign_render_options(m_render_options);
    //m_lines.set_content(U"'l'l\nd");

    m_grid.assign_font(font);
    m_render_options.set_text_selection(m_user_selection);

    add_widget(&m_grid);
    m_interface.reset(new KsgTextGrid::TargetInterface(m_grid));

    m_elapsed_time_grid = m_interface->make_sub_grid(Cursor(0, 0), SubTextGrid::REST_OF_GRID, 1);
    m_doc = m_interface->make_sub_grid(Cursor(1, 0));

    m_lines.render_to(m_doc, bottom_offset(m_lines,  m_doc));
    set_title_visible(false);
    set_style(styles);
    update_geometry();
}

void EditorDialog::process_event(const sf::Event & event) {
    Frame::process_event(event);
    auto old_selection = m_user_selection;
    handle_event(&m_user_selection, &m_lines, event);
    if (old_selection != m_user_selection) {
        m_render_options.set_text_selection(m_user_selection);
        m_render_options.toggle_cursor_flash();
        m_lines.render_to(m_doc, bottom_offset(m_lines,  m_doc));
    }
}

void EditorDialog::do_update(float et, TextTyperBot & bot) {
    bool requires_rerender = false;
    requires_rerender = (bot.update(m_lines, m_user_selection, double(et)) == TextTyperBot::HAS_UPDATE);
    m_delay += et;

    {
    static float min = std::numeric_limits<float>::min();
    static float max = std::numeric_limits<float>::max();
    min = std::max(min, et);
    max = std::min(max, et);
    TextLine tline(expand_char_width(std::to_string(min) + " " + std::to_string(max)));
    tline.constrain_to_width(m_elapsed_time_grid.width());
    tline.render_to(m_elapsed_time_grid, 0);
    }

    if (m_delay > 0.3f) {
        m_delay = 0.f;
        m_render_options.toggle_cursor_flash();
        requires_rerender |= true;
    }
    //if (requires_rerender) {
        m_render_options.set_text_selection(m_user_selection);
        m_lines.render_to(m_doc, bottom_offset(m_lines,  m_doc));
    //}
}

TextTyperBot::TextTyperBot():
    m_current_index(0  ),
    m_type_rate    (0.0),
    m_delay        (0.0)
{}
decltype (TextTyperBot::NO_UPDATE) TextTyperBot::update
    (TextLines & lines, UserTextSelection & textsel, double et)
{
    if (m_type_rate == 0.0 || m_content.empty()) return NO_UPDATE;
    m_delay += et;
    auto rv = NO_UPDATE;
    while (m_delay > m_type_rate) {
        textsel.push(&lines, m_content[m_current_index]);
        rv = HAS_UPDATE;
        if (++m_current_index == m_content.size()) {
            m_content.clear();
            break;
        }
        m_delay -= m_type_rate;
    }
    return rv;
}
TextTyperBot & TextTyperBot::set_content(const std::u32string & content) {
    m_content = content;
    return *this;
}

TextTyperBot & TextTyperBot::set_type_rate(double rate) {
    m_type_rate = rate;
    return *this;
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
    (UserTextSelection * selection, TextLines * tlines,
     const sf::Event & event)
{
    assert(selection);
    auto update_hold_alt = [&] () {
        if (event.key.shift)
            selection->hold_alt_cursor();
        else
            selection->release_alt_cursor();
    };
    switch (event.type) {
    case sf::Event::KeyReleased:
        update_hold_alt();
        break;
    case sf::Event::KeyPressed:
        update_hold_alt();
        switch (event.key.code) {
        case sf::Keyboard::Down:
            selection->move_down(*tlines);
            break;
        case sf::Keyboard::Up:
            selection->move_up(*tlines);
            break;
        case sf::Keyboard::Left:
            selection->move_left(*tlines);
            break;
        case sf::Keyboard::Right:
            selection->move_right(*tlines);
            break;
        case sf::Keyboard::Delete:
            selection->delete_ahead(tlines);
            break;
        case sf::Keyboard::BackSpace:
            selection->delete_behind(tlines);
            break;
        case sf::Keyboard::Return:
            selection->push(tlines, TextLines::NEW_LINE);
            break;
        default:break;
        }
        break;
    case sf::Event::TextEntered:
        if (event.text.unicode == 8 || event.text.unicode == 127 || event.text.unicode == 13) break;
        selection->push(tlines, UChar(event.text.unicode));
        break;
    default: break;
    }
}

std::u32string load_ascii_textfile(const char * filename) {
    std::ifstream fin(filename);
    auto fbeg = fin.tellg();
    fin.seekg(fin.end);
    auto fend = fin.tellg();
    auto length = fend - fbeg;
    fin.seekg(fin.beg);
    std::u32string content;
    content.reserve(std::size_t(length));
    while (fin) {
        char c;
        fin.read(&c, 1);
        content.push_back(UChar(c));
    }
    return content;
}

int bottom_offset(const TextLines & textlines, const TargetTextGrid & text_grid) {
    int height_so_far = 0;
    for (const auto & line : textlines.lines()) {
        height_so_far += line.height_in_cells();
    }
    return -std::max(0, height_so_far - text_grid.height());
}
