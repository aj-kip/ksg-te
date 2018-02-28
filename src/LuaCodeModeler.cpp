/****************************************************************************

    File: LuaCodeModeler.cpp
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

#include "LuaCodeModeler.hpp"
#include "TextLines.hpp"

#include <stdexcept>
#include <set>

#include <cassert>

namespace {

using UStringCIter = LuaCodeModeler::UStringCIter;
using CharTestFunc = bool (*)(UChar);

const std::set<std::u32string> & get_lua_keywords();
const std::set<std::u32string> & get_lua_constants();

constexpr const int NOT_MULTILINE = -1;
int get_mutliline_size(UStringCIter);
int get_multiline_end_size(UStringCIter);
UStringCIter get_end_of_numeric(UStringCIter);
// doesn't if ends with new_line or null
bool string_terminates(UStringCIter);

template <CharTestFunc test_func>
LuaCodeModeler::Response handle_regular_sequence(UStringCIter);

bool is_alphanum(UChar);
bool is_whitespace(UChar);

} // end of <anonymous> namespace

LuaCodeModeler::LuaCodeModeler():
    m_in_comment  (false),
    m_current_string_quote   (NOT_IN_STRING),
    m_string_terminates(false),
    m_in_multiline_size(NOT_MULTILINE)
{}

void LuaCodeModeler::reset_state() {
    m_in_comment = m_string_terminates = false;
    m_current_string_quote = NOT_IN_STRING;
    m_in_multiline_size = NOT_MULTILINE;
}

LuaCodeModeler::Response LuaCodeModeler::update_model
    (UStringCIter itr, Cursor)
{
    if (m_in_multiline_size != NOT_MULTILINE)
        return handle_multiline(itr);
    if (m_in_comment)
        return handle_comment(itr);
    if (m_current_string_quote != NOT_IN_STRING)
        return handle_string(itr);
    // default mode
    // gets complicated here
    switch (*itr) {
    case 0: return make_resp(itr, 0, false);
    // arithmetic operators
    case U'+': case U'*': case U'/': case U'%': case U'^':
        return make_resp(itr + 1, OPERATOR, false);
    case U'-': // covers unary also, combines with "-"
        if (*(itr + 1) == U'-') {
            m_in_comment = true;
            return make_resp(itr + 2, COMMENT, false);
        }
        return make_resp(itr + 1, OPERATOR, false);
    // relational operators
    case U'<': // check also multichar <=
    case U'>': // also >=
    // almighty assignment operator
    case U'=': // also ==
    case U'~': // combines only with another "="
        if (*(itr + 1) == U'=')
            return make_resp(itr + 2, OPERATOR, false);
        return make_resp(itr + 1, OPERATOR, false);
    case U'.': // combines only another ".", and maybe even a 3rd "."
        if (*(itr + 1) == U'.') {
            if (*(itr + 2) == U'.')
                return make_resp(itr + 3, OPERATOR, false);
            return make_resp(itr + 2, OPERATOR, false);
        }
        return make_resp(itr + 1, OPERATOR, false);
    case U'#': case U']':
        return make_resp(itr + 1, OPERATOR, false);
    case U':': // combines only with another ":"
        if (*(itr + 1) == U':')
            return make_resp(itr + 2, OPERATOR, false);
        return make_resp(itr + 1, OPERATOR, false);
    // squares may be part of a variable width comment "token", so for sanity's
    // sake, I'll consider "[====[" as one sequence like any alphanumeric
    case U'[':
        m_in_multiline_size = get_mutliline_size(itr);
        if (NOT_MULTILINE == m_in_multiline_size)
            return make_resp(itr + 1, OPERATOR, false);
        return make_resp(itr + m_in_multiline_size, STRING, false);
    // other
    case U'{': case U'}': case U'(': case U')':
    case U',': case U';': case U'\\':
        // strings, which need to be "tokenized"/broken into words inside the
        // actual string content, they can be "recombined" into a proper token
        // later
        return make_resp(itr + 1, OPERATOR, false);
    case U'"': case U'\'':
        m_current_string_quote = *itr;
        m_string_terminates = string_terminates(itr);
        return make_resp
            (itr + 1, m_string_terminates ? STRING : UNTERMINATE_STRING, false);
    case U'0': case U'1': case U'2': case U'3': case U'4':
    case U'5': case U'6': case U'7': case U'8': case U'9':
        return make_resp(get_end_of_numeric(itr), NUMERIC, false);
    case U' ': case U'\t': case U'\r':
        return make_resp(handle_regular_sequence<is_whitespace>(itr));
    // assume alphanumeric on default
    default: break;
    };
    auto rv = make_resp(handle_regular_sequence<is_alphanum>(itr));
    rv.token_type = identify_alphanum(itr, rv.next);
    return rv;
}

/* static */ ColorPair LuaCodeModeler::colors_for_pair(int pid) {
    auto with_default_back = [](uint8_t r, uint8_t g, uint8_t b) {
        static const sf::Color default_back_c = sf::Color(20, 20, 20);
        return ColorPair(sf::Color(r, g, b), default_back_c);
    };
    auto with_warn_back = [](uint8_t r, uint8_t g, uint8_t b) {
        static const sf::Color warn_back_c = sf::Color(60, 20, 20);
        return ColorPair(sf::Color(r, g, b), warn_back_c);
    };
    switch (pid) {
    case REGULAR_CODE       : return with_default_back(255, 255, 255);
    case OPERATOR           : return with_default_back(255, 100, 100);
    case STRING             : return with_default_back(255, 255,   0);
    case COMMENT            : return with_default_back(  0, 200, 200);
    case KEYWORD            : return with_default_back(200, 150,   0);
    case NUMERIC            : return with_default_back(255, 150, 150);
    case LEADING_WHITESPACE : return with_warn_back   (255, 255, 255);
    case KEY_CONSTANTS      : return with_default_back(255,   0, 200);
    case UNTERMINATE_STRING : return with_warn_back   (255, 100, 100);
    case BAD_MULTILINE      : return with_warn_back   (255, 255,   0);
    default: throw std::invalid_argument("LuaCodeModeler::colors_for_pair: "
                                         "given pair ID not valid.");
    }
}

/* private */ LuaCodeModeler::Response LuaCodeModeler::handle_multiline
    (UStringCIter itr)
{
    int end_size = get_multiline_end_size(itr);
    if (end_size == m_in_multiline_size) {
        m_in_multiline_size = NOT_MULTILINE;
        return make_resp(itr + end_size, STRING, false);
    }

    // reminder: CodeModeler's default instance is stateless!
    auto rv_resp = CodeModeler::default_instance().update_model(itr, Cursor());
    bool stray_mutliline = end_size != NOT_MULTILINE;
    rv_resp.token_type = stray_mutliline ? BAD_MULTILINE : STRING;
    if (stray_mutliline)
        rv_resp.next = itr + end_size;
    return make_resp(rv_resp);
}

/* private */ LuaCodeModeler::Response LuaCodeModeler::handle_comment
    (UStringCIter itr)
{
    // done all the way to the end of the line
    m_in_comment = *itr != 0 && *itr != TextLines::NEW_LINE;
    if (!m_in_comment) {
        return make_resp(itr + 1, COMMENT, false);
    }
    // reminder: CodeModeler's default instance is stateless!
    auto rv_resp = CodeModeler::default_instance().update_model(itr, Cursor());

    rv_resp.token_type = COMMENT;
    return make_resp(rv_resp);
}

/* private */ LuaCodeModeler::Response LuaCodeModeler::handle_string
    (UStringCIter itr)
{
    auto rv_type = m_string_terminates ? STRING : UNTERMINATE_STRING;
    // first be careful, check if it's the end of the string or line
    if (*itr == m_current_string_quote) {
        m_current_string_quote = NOT_IN_STRING;
        return make_resp(itr + 1, rv_type, false);
    } else if (*itr == U'\\' && *(itr + 1) == m_current_string_quote) {
        return make_resp(itr + 2, rv_type, false);
    }
    // if not, we'll apply the hard, fast, and sloppy rules
    auto rv = CodeModeler::default_instance().update_model(itr, Cursor());
    rv.token_type = rv_type;
    return make_resp(rv);
}

/* private */ int LuaCodeModeler::identify_alphanum
    (UStringCIter beg, UStringCIter end) const
{
    std::u32string temp(beg, end);
    auto itr = get_lua_keywords().find(temp);
    if (itr != get_lua_keywords().end())
        return KEYWORD;
    itr = get_lua_constants().find(temp);
    if (itr != get_lua_constants().end())
        return KEY_CONSTANTS;
    return REGULAR_CODE;
}

/* private */ void LuaCodeModeler::check_invarients() const {
    // nothing to check...
}


namespace {

template <UChar SQUARE_BRACKET>
int get_multiline_size_general(UStringCIter itr);

bool is_operator(UChar);

int get_mutliline_size(UStringCIter itr)
    { return get_multiline_size_general<U'['>(itr); }

int get_multiline_end_size(UStringCIter itr)
    { return get_multiline_size_general<U']'>(itr); }

UStringCIter get_end_of_numeric(UStringCIter itr) {
    bool saw_dot = false;
    assert(*itr == U'0' || *itr == U'1' || *itr == U'2' || *itr == U'3' ||
           *itr == U'4' || *itr == U'5' || *itr == U'6' || *itr == U'7' ||
           *itr == U'8' || *itr == U'9');
    for (; true; ++itr) {
        switch (*itr) {
        case 0: case TextLines::NEW_LINE: return itr;
        case U'0': case U'1': case U'2': case U'3': case U'4':
        case U'5': case U'6': case U'7': case U'8': case U'9':
            break;
        case U'.':
            if (saw_dot) return itr;
            saw_dot = true;
            break;
        default: return itr;
        }
    }
}

// doesn't if ends with new_line or null
bool string_terminates(UStringCIter itr) {
    assert(*itr == U'"' || *itr == U'\'');
    UChar quotation = *itr++;
    while (*itr != 0 && *itr != TextLines::NEW_LINE) {
        if (*itr == quotation) return true;
        ++itr;
    }
    return false;
}

template <CharTestFunc test_func>
LuaCodeModeler::Response handle_regular_sequence(UStringCIter itr) {
    assert(test_func(*itr));
    while (*itr != 0 && test_func(*itr)) ++itr;
    return LuaCodeModeler::Response
        { itr, LuaCodeModeler::REGULAR_CODE, (test_func == is_whitespace) };
}

bool is_alphanum(UChar uchr) {
    return !is_operator(uchr) && !is_whitespace(uchr);
}

bool is_whitespace(UChar uchr) {
    return uchr == U' ' || uchr == U'\t' || uchr == U'\r' || uchr == U'\n';
}

const std::set<std::u32string> & get_lua_keywords() {
    static std::set<std::u32string> keywords({
         U"and"   , U"break" , U"do"      , U"else" , U"elseif",
         U"end"   , U"for"   , U"function", U"if"   , U"while",
         U"in"    , U"local" , U"not"     , U"or"   ,
         U"repeat", U"return", U"then"    , U"until"
     });
    return keywords;
}

const std::set<std::u32string> & get_lua_constants() {
    static std::set<std::u32string> constants({
        U"false", U"nil" , U"true"
    });
    return constants;
}

// ----------------------------------------------------------------------------

template <UChar SQUARE_BRACKET>
int get_multiline_size_general(UStringCIter itr) {
    if (*itr != SQUARE_BRACKET) return NOT_MULTILINE;
    int rv = 2; // include the square brackets
    while (*++itr == U'=') ++rv;
    if (*itr != SQUARE_BRACKET) return NOT_MULTILINE;
    return rv;
}

bool is_operator(UChar uchr) {
    switch (uchr) {
    case U'`': case U'~': case U'!': case U'@': case U'$': case U'%':
    case U'^': case U'*': case U'(': case U')': case U'-': case U'=':
    case U'+':
    case U'[': case U']': case U'\\': case U';': case U'/': case U',':
    case U'.': case U'{': case U'}': case U'|': case U':': case U'"':
    case U'<': case U'>': case U'?': return true;
    default: return false;
    }
}

} // end of <anonymous> namespace
