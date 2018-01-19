#pragma once

#include <SFML/Graphics/Color.hpp>

#include <set>

#include "TextLines.hpp"

class TextGrid;

/** A necessary complexity to aid unit testing.
 *
 */
class TextRendererPriv {
    friend class TextRenderer;
    friend class TextWriterDefs;
    // maps text line cursors to text grid cursors
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

class UserTextSelection {
public:
    UserTextSelection(): m_alt_held(false) {}

    // events to move primary
    void move_left(const TextLines &);
    void move_right(const TextLines &);
    void move_down(const TextLines &);
    void move_up(const TextLines &);
    void page_down(const TextLines &, int page_size);
    void page_up(const TextLines &, int page_size);

    // events controlling alt
    void hold_alt_cursor   () { m_alt_held = true ; }
    void release_alt_cursor() { m_alt_held = false; }

    bool is_in_range(Cursor) const;

    Cursor begin() const;
    Cursor end() const;
private:
    void constrain_primary_update_alt(const TextLines &);
    bool primary_is_ahead() const;

    bool m_alt_held;
    Cursor m_primary;
    Cursor m_alt;
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
    using KeywordMap = std::set<std::u32string>;
    struct HighlightInfo;
    // allows for unit tests
    using GridWriter = TextRendererPriv::GridWriter;

    struct HighlightInfo {
        KeywordMap keywords;
        sf::Color keyword_fore;
        sf::Color default_fore;
        sf::Color default_back;
        sf::Color select_fore;
        sf::Color select_back;
    };

    /**
     *  Behavior guarentees for GridWriter:
     *  - For newlines, the single char write function will always be called.
     *  - There is always enough room in the current line to write the entire
     *    string
     *  @param starting_point
     */
    void render_lines_to
        (GridWriter &, Cursor starting_point, const TextLines &,
         const UserTextSelection & = UserTextSelection());

    Cursor render_word_to
        (GridWriter &, Cursor grid_pos, const std::u32string &,
         const UserTextSelection & = UserTextSelection());

    void fill_remainder_with_blanks(GridWriter &, Cursor grid_pos);
    HighlightInfo m_info;
};

inline void UserTextSelection::move_left(const TextLines & tlines) {
    if (m_primary == tlines.end_cursor()) return;
    m_primary = tlines.next_cursor(m_primary);
    if (!m_alt_held)
        m_alt = m_primary;
}

inline void UserTextSelection::move_right(const TextLines & tlines) {
    if (m_primary == Cursor()) return;
    m_primary = tlines.previous_cursor(m_primary);
    if (!m_alt_held)
        m_alt = m_primary;
}

inline void UserTextSelection::move_down(const TextLines & tlines) {
    ++m_primary.line;
    constrain_primary_update_alt(tlines);
}

inline void UserTextSelection::move_up(const TextLines & tlines) {
    --m_primary.line;
    constrain_primary_update_alt(tlines);
}

inline void UserTextSelection::page_down
    (const TextLines & tlines, int page_size)
{
    m_primary.line += page_size;
    constrain_primary_update_alt(tlines);
}

inline void UserTextSelection::page_up
    (const TextLines & tlines, int page_size)
{
    m_primary.line -= page_size;
    constrain_primary_update_alt(tlines);
}

inline bool UserTextSelection::is_in_range(Cursor cursor) const {
    auto end_ = end(), beg = begin();
    return cursor.line >= beg.line    && cursor.column >= beg.column &&
           cursor.line <  end_.column && cursor.column <  end_.column;
}

inline Cursor UserTextSelection::begin() const {
    return primary_is_ahead() ? m_alt : m_primary;
}

inline Cursor UserTextSelection::end() const {
    return primary_is_ahead() ? m_primary : m_alt;
}

/* private */ inline void UserTextSelection::constrain_primary_update_alt
    (const TextLines & tlines)
{
    m_primary = tlines.constrain_cursor(m_primary);
    if (!m_alt_held)
        m_alt = m_primary;
}

/* private */ inline bool UserTextSelection::primary_is_ahead() const {
    if (m_primary.line > m_alt.line) return true;
    return m_primary.column > m_alt.column;
}
