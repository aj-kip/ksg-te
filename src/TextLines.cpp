#include "TextLines.hpp"

#include <stdexcept>

#include <cassert>

TextLines::CharReader::~CharReader() {}

void TextLines::CharReader::read_char(UChar c, Cursor)
    { read_char(c); }

Cursor TextLines::push(Cursor cur, UChar c) {
    verify_cursor_validity("TextLines::push", cur);
    if (m_text.empty()) {
        m_text.emplace_back();
    }
    auto line_itr = m_text.begin() + cur.line;
    auto col_itr  = line_itr->begin() + cur.column;
    track_char_identity(c, CHAR_ADDED);
    if (c == NEW_LINE) {
        m_text.insert(line_itr + 1, std::u32string(col_itr, line_itr->end()));
        line_itr->erase(col_itr, line_itr->end());
        return Cursor(cur.line + 1, 0);
    } else {
        line_itr->insert(col_itr, c);
        return Cursor(cur.line, cur.column + 1);
    }
}

Cursor TextLines::delete_ahead(Cursor cur) {
    verify_cursor_validity("TextLines::delete_ahead", cur);

    // check availablity
    if (end_cursor() == cur) return cur;

    auto line_itr = m_text.begin()    + cur.line  ;
    auto col_itr  = line_itr->begin() + cur.column;
    track_char_identity(*col_itr, CHAR_REMOVED);

    // end of line? col_itr can easily be end of line
    if (col_itr == line_itr->end()) {
        // merge to next line if exists
        if ((line_itr + 1) != m_text.end()) {
            auto next_line = line_itr + 1;
            line_itr->insert(col_itr, next_line->begin(), next_line->end());
            //line_itr->erase(line_itr->end() - 1);
            m_text.erase(next_line);
            return cur;
        } else {
            return cur;
        }
    } else {
        line_itr->erase(col_itr);
    }
    return cur;
}

Cursor TextLines::delete_behind(Cursor cur) {
    verify_cursor_validity("TextLines::delete_behind", cur);
    if (cur == Cursor(0, 0)) return cur;

    auto line_itr = m_text.begin()    + cur.line  ;
    auto col_itr  = line_itr->begin() + cur.column;
    track_char_identity(*col_itr, CHAR_REMOVED);

    if (col_itr == line_itr->begin()) {
        // merge to previous, assumed to be available
        assert(line_itr != m_text.begin());
        auto prev_line = line_itr - 1;
        auto new_col   = int(prev_line->size());
        prev_line->insert(prev_line->end(), line_itr->begin(), line_itr->end());
        m_text.erase(line_itr);
        return Cursor(cur.line - 1, new_col);
    } else {
        line_itr->erase(col_itr - 1);
        return Cursor(cur.line, cur.column - 1);
    }
}

Cursor TextLines::wipe(Cursor beg, Cursor end) {
    verify_cursor_validity("TextLines::wipe", beg);
    verify_cursor_validity("TextLines::wipe", end);
    verify_cursors_not_crossing("TextLines::wipe", beg, end);

    if (end.line == beg.line) {
        auto & line = m_text[std::size_t(end.line)];
        track_char_identity(line.begin() + beg.column, line.begin() + end.column, CHAR_REMOVED);
        line.erase(line.begin() + beg.column, line.begin() + end.column);
        return beg;
    }

    // generate sequence for both lines and columns to erase
    // wipe all "inbetweeners", end must be adjusted by offset of the
    // difference minus one
    if (end.line - beg.line > 1) {
        // watch OBOE
        auto num_removed = end.line - beg.line - 1;
        auto beg_line = m_text.begin() + beg.line + 1;
        auto end_line = m_text.begin() + end.line;
        for (auto itr = beg_line; itr != end_line; ++itr) {
            track_char_identity(itr->begin(), itr->end(), CHAR_REMOVED);
        }
        m_text.erase(beg_line, end_line);
        end.line -= num_removed;
    }
    assert(end.line - beg.line == 1);
    // from beg to end of host line -> remove
    // from end to beg of host line -> remove
    // merge the two seperate lines
    auto beg_line_itr = m_text.begin() + beg.line;
    auto end_line_itr = m_text.begin() + end.line;
    auto beg_line_first_erased = beg_line_itr->begin() + beg.column;
    auto end_line_end_of_erased = end_line_itr->begin() + end.column;

    track_char_identity(beg_line_first_erased, beg_line_itr->end(), CHAR_REMOVED);
    track_char_identity(end_line_itr->begin(), end_line_end_of_erased, CHAR_REMOVED);

    beg_line_itr->erase(beg_line_first_erased, beg_line_itr->end());
    end_line_itr->erase(end_line_itr->begin(), end_line_end_of_erased);

    beg_line_itr->insert(beg_line_itr->end(),
                         end_line_itr->begin(), end_line_itr->end());
    m_text.erase(end_line_itr);
    return beg;
}

std::u32string TextLines::withdraw_characters_from
    (Cursor beg, Cursor end) const
{
    struct StringBuilder final : public CharReader {
        void read_char(UChar c) override { rv += c; }
        std::u32string rv;
    };
    StringBuilder string_builder;
    read_characters("TextLines::withdraw_characters_from", string_builder, beg, end);
    return string_builder.rv;
}

Cursor TextLines::deposit_chatacters_to
    (const UChar * beg, const UChar * end, Cursor pos)
{
    for (auto itr = beg; itr != end; ++itr)
        pos = push(pos, *itr);
    return pos;
}

bool TextLines::holds_at_most_ascii_characters() const
    { return m_non_ascii_count == 0; }

UChar TextLines::read_character(Cursor cur) const {
    verify_cursor_validity("TextLines::read_character", cur);
    const auto & line = *(m_text.begin() + cur.line);
    if (cur.column == int(line.size())) return NEW_LINE;
    return line[std::size_t(cur.column)];
}

Cursor TextLines::next_cursor(Cursor cur) const {
#   ifndef NDEBUG
    verify_cursor_validity("TextLines::next_cursor", cur);
#   endif
    const auto & line = *(m_text.begin() + cur.line);
    if (cur.column == int(line.size()))
        cur = Cursor(cur.line + 1, 0);
    else
        cur = Cursor(cur.line, cur.column + 1);

    return cur;
}

Cursor TextLines::previous_cursor(Cursor cursor) const {
#   ifndef NDEBUG
    verify_cursor_validity("TextLines::previous_cursor", cursor);
#   endif
    if (cursor.column-- == -1) {
        cursor.column = int(m_text[std::size_t(cursor.line)].size()) - 1;
        --cursor.line;
    }
    return cursor;
}

Cursor TextLines::constrain_cursor(Cursor cursor) const noexcept {
    if (cursor.line < 0) {
        cursor.line = 0;
    } else if (cursor.line >= int(m_text.size())) {
        cursor.line = int(m_text.size()) - 1;
    }
    auto line_len = int(m_text[std::size_t(cursor.line)].size());
    if (cursor.column < 0) {
        cursor.column = 0;
    } else if (cursor.column >= line_len) {
        cursor.column = line_len;
    }
    return cursor;
}

Cursor TextLines::end_cursor() const {
    if (m_text.empty()) return Cursor(0, 0);
    return Cursor(int(m_text.size() - 1), int(m_text.back().size()));
}

/* private */ void TextLines::verify_cursor_validity
    (const char * funcname, Cursor cur) const
{
    auto throw_line_out_of_range = [funcname]() {
        throw std::out_of_range(std::string(funcname)
                                + ": Cursor line is out of range.");
    };
    if (cur.line < 0 || cur.column < 0) {
        throw std::invalid_argument(std::string(funcname)
                                    + ": invalid cursor (negative indicies)");
    }
    if (m_text.empty() && cur == Cursor(0, 0)) {
        return;
    }
    if (cur.line >= int(m_text.size())) throw_line_out_of_range();
    if (cur.column > int(m_text[std::size_t(cur.line)].size()))
        throw_line_out_of_range();
}

/* private */ void TextLines::verify_cursors_not_crossing
    (const char * funcname, Cursor beg, Cursor end) const
{
    if (end.line < beg.line || (end.line == beg.line && end.column < beg.column)) {
        throw std::invalid_argument(
            std::string(funcname) + ": begining cursor is further than the "
            "end cursor, which is an invalid selection.");
    }
}

/* private */ void TextLines::track_char_identity
    (UChar c, decltype(CHAR_ADDED) action)
{
    assert(action == CHAR_REMOVED || action == CHAR_ADDED);
    int off = (action == CHAR_ADDED) ? 1 : -1;
    if (!is_ascii(c)) {
        m_non_ascii_count += off;
    }
}

/* private */ void TextLines::read_characters
    (const char * caller, CharReader & char_reader, Cursor beg, Cursor end) const
{
    verify_cursor_validity(caller, beg);
    verify_cursor_validity(caller, end);
    verify_cursors_not_crossing(caller, beg, end);
    auto next_cursor = [this, caller](Cursor cur) {
        verify_cursor_validity(caller, cur);
        auto line_itr = m_text.begin() + cur.line;
        auto col_itr  = line_itr->begin() + cur.column;
        if (col_itr == line_itr->end()) {
            return Cursor(cur.line + 1, 0);
        } else {
            return Cursor(cur.line, cur.column + 1);
        }
    };
    for (auto itr = beg; itr != end; itr = next_cursor(itr)) {
        const auto & line = m_text[std::size_t(itr.line)];
        if (itr.column == int(line.size()))
            char_reader.read_char(NEW_LINE, itr);
        else
            char_reader.read_char(line[std::size_t(itr.column)], itr);
    }
}

template <typename CharIter>
/* private */ void TextLines::track_char_identity
    (CharIter beg, CharIter end, decltype(CHAR_ADDED) action)
{
    static_assert(std::is_same<UChar, typename std::remove_reference<decltype (*beg)>::type>::value, "");
    for (auto itr = beg; itr != end; ++itr) {
        track_char_identity(*itr, action);
    }
}

/* static */ void TextLines::run_test() {
    // push features
    auto push_string =
        [](TextLines * lines, const UChar * const str, Cursor cur)
    {
        for (auto c : std::u32string(str)) {
            cur = lines->push(cur, c);
        }
        return cur;
    };
    {
    TextLines tlines;
    Cursor c(0, 0);
    tlines.push(c, U' ');
    assert(tlines.end_cursor() == Cursor(0, 1));
    }
    {
    TextLines tlines;
    Cursor c(0, 0);
    tlines.push(c, TextLines::NEW_LINE);
    assert(tlines.end_cursor() == Cursor(1, 0));
    }
    {
    TextLines tlines;
    Cursor cur(0, 0);
    cur = tlines.push(cur, TextLines::NEW_LINE);
    cur = push_string(&tlines, U"Howdy neighbor", cur);
    assert(tlines.end_cursor() == cur);
    }
    // withdraw_characters_from
    {
    static constexpr const UChar * const TEXT = U"Howdy Neighbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    assert(tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor()) == TEXT);
    }
    {
    static constexpr const UChar * const TEXT = U"Howdy\nNeighbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    assert(tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor()) == TEXT);
    }
    {
    static constexpr const UChar * const TEXT = U"How\ndy Nei\nghbor?";
    TextLines tlines;
    push_string(&tlines, TEXT, Cursor(0, 0));
    assert(tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor()) == TEXT);
    }
    // delete ahead/behind
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_ahead(cur);
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"ab\nab" && cur == Cursor(0, 2));
    }
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_behind(cur);
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"ac\nab" && cur == Cursor(0, 1));
    }
    {
    TextLines tlines;
    push_string(&tlines, U"abc\nab", Cursor(0, 0));
    Cursor cur(0, 2);
    cur = tlines.delete_ahead(tlines.delete_ahead(cur));
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"abab" && cur == Cursor(0, 2) && tlines.end_cursor().line == 0);
    }
    // wipe
    {
    TextLines tlines;
    push_string(&tlines, U"How now brown cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(0, 14));
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    {
    TextLines tlines;
    push_string(&tlines, U"How now bro\nwn cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(1, 3));
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    {
    TextLines tlines;
    push_string(&tlines, U"How now bro\nwn\nabc cow.", Cursor(0, 0));
    tlines.wipe(Cursor(0, 8), Cursor(2, 4));
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == U"How now cow.");
    }
    // deposit_chatacters_to
    {
    static const std::u32string utext = U"Howdy Neighbor";
    TextLines tlines;
    tlines.deposit_chatacters_to(utext.data(), utext.data() + utext.length());
    auto ustr = tlines.withdraw_characters_from(Cursor(0, 0), tlines.end_cursor());
    assert(ustr == utext);
    }
    // holds_at_most_ascii_characters
}
