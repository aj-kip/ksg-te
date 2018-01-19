#include <vector>
#include <string>

#pragma once

using UChar = char32_t;

struct Cursor {
    Cursor(): line(0), column(0) {}

    Cursor(int line_, int column_): line(line_), column(column_) {}

    bool operator == (const Cursor & lhs)
        { return line == lhs.line && column == lhs.column; }

    bool operator != (const Cursor & lhs)
        { return line != lhs.line || column != lhs.column; }

    int line;
    int column;
};

class TextLines {
public:
    using ConstUCharIter = std::vector<UChar>::const_iterator;

    static constexpr const UChar NEW_LINE = U'\n';

    static bool is_ascii(UChar u) { return (u & 0x7F) == u; }
    static void run_test();

    Cursor push(Cursor, UChar);
    // user presses "del"
    Cursor delete_ahead(Cursor);
    // user presses "backspace"
    Cursor delete_behind(Cursor);
    Cursor wipe(Cursor beg, Cursor end);
    std::u32string withdraw_characters_from(Cursor beg, Cursor end) const;
    void deposit_chatacters_to
        (ConstUCharIter beg, ConstUCharIter end, Cursor pos = Cursor());
    Cursor deposit_chatacters_to
        (const UChar * beg, const UChar * end, Cursor pos = Cursor());
    bool holds_at_most_ascii_characters() const;

    UChar read_character(Cursor) const;
    Cursor next_cursor(Cursor) const;
    Cursor previous_cursor(Cursor) const;
    Cursor constrain_cursor(Cursor) const noexcept;
    Cursor end_cursor() const;

private:
    struct CharReader {
        virtual ~CharReader();
        virtual void read_char(UChar) = 0;
        virtual void read_char(UChar, Cursor);
    };

    enum { CHAR_REMOVED, CHAR_ADDED };

    void verify_cursor_validity(const char * funcname, Cursor) const;

    void verify_cursors_not_crossing
        (const char * funcname, Cursor beg, Cursor end) const;

    void track_char_identity(UChar, decltype(CHAR_ADDED));

    void read_characters(const char * caller,
                         CharReader &, Cursor beg, Cursor end) const;

    template <typename CharIter>
    void track_char_identity(CharIter beg, CharIter end, decltype(CHAR_ADDED));

    std::vector<std::u32string> m_text;
    int m_non_ascii_count;
};

inline void TextLines::deposit_chatacters_to
    (ConstUCharIter beg, ConstUCharIter end, Cursor pos)
{
    if (beg == end) return;
    deposit_chatacters_to(&*beg, &*(end - 1) + 1, pos);
}
