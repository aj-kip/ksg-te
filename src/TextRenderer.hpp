/****************************************************************************

    File: TextRenderer.hpp
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

#include <SFML/Graphics/Color.hpp>

#include <set>

#include "TextLines.hpp"
#include "UserTextSelection.hpp"

class TextGrid;

/** A necessary complexity to aid unit testing.
 *
 */
class TextRendererPriv {
    friend class TextRenderer;
    friend class TextWriterDefs;
    // maps text line cursors to text grid cursors
    /**
    *  Behavior guarentees for GridWriter:
    *  - For newlines, the single char write function will always be called.
    *  - - cursors for the new line must correspond to the old line
    *  - There is always enough room in the current line to write the entire
    *    string
    *  @param starting_point
    */
    class GridWriter {
    public:
        virtual ~GridWriter();
        virtual void write(sf::Color fore, sf::Color back, Cursor, const std::u32string &) = 0;
        virtual void write(sf::Color fore, sf::Color back, Cursor, UChar) = 0;
        virtual int width() const = 0;
        virtual int height() const = 0;
        Cursor next_cursor(Cursor) const;
        Cursor end_cursor() const;
    protected:
        void verify_cursor_validity(const char * funcname, Cursor) const;
    };
};

/** Highlighting without scope, simple keyword -> highlight + default colors
 *
 */
class TextRenderer {
public:
    static const sf::Color default_keyword_fore_c;
    static const sf::Color default_keyword_back_c;
    static const sf::Color default_fore_c;
    static const sf::Color default_back_c;
    static const sf::Color default_select_fore_c;
    static const sf::Color default_select_back_c;
    static constexpr const int DEFAULT_TAB_WIDTH = 4;
    TextRenderer();
    void setup_defaults();
    // I've neglected some important details
    // A cursor selection range needs to be considered when rendering the text
    // another nice to have: re-render only portions of the text at a time
    // (a line at a time) rather than the whole thing
    void render_lines_to(TextGrid *, Cursor starting_point, const TextLines &);
    void render_lines_to
        (TextGrid *, Cursor starting_point, const TextLines &,
         const UserTextSelection &);


    void add_keyword(const std::u32string &);

    static void run_tests();
    static bool is_whitespace(UChar);
    static sf::Color invert(sf::Color);
private:
    // allows for unit tests
    using GridWriter = TextRendererPriv::GridWriter;
    using KeywordMap = std::set<std::u32string>;
    struct HighlightInfo;


    struct HighlightInfo {
        KeywordMap keywords;
        sf::Color keyword_fore;
        sf::Color default_fore;
        sf::Color default_back;
        sf::Color select_fore;
        sf::Color select_back;
    };

    struct CursorPair {
        CursorPair(): in_text_lines(true) {}
        Cursor textlines;
        Cursor grid;
        bool in_text_lines;
    };

    void render_lines_to
        (GridWriter &, Cursor starting_point, const TextLines &,
         const UserTextSelection & selection = UserTextSelection());

    Cursor render_word_to
        (GridWriter &, const CursorPair &, const std::u32string &,
         const UserTextSelection & selection, const TextLines &);

    Cursor render_word_hard_wrap
        (GridWriter &, const CursorPair &, const std::u32string &,
         const UserTextSelection & selection, const TextLines &);

    Cursor render_word_soft_wrap
        (GridWriter &, const CursorPair &, const std::u32string &,
         const UserTextSelection & selection, const TextLines &);

    Cursor render_word_no_wrap
        (GridWriter &, const CursorPair &, const std::u32string &,
         const UserTextSelection & selection, const TextLines &);

    Cursor print_space(GridWriter & writer, const CursorPair & cursors,
                       const UserTextSelection & selection) const;

    Cursor fill_remainder_line_with_blanks
        (GridWriter & writer, const CursorPair & cursors) const;

    void fill_remainder_with_blanks(GridWriter &, Cursor grid_pos);
    HighlightInfo m_info;
};
