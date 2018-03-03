/****************************************************************************

    File: TextLine.cpp
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

#include "TextLine.hpp"
#include "TextLines.hpp"

#include <limits>
#include <cassert>

namespace {

using UStringCIter = TextLines::UStringCIter;

bool is_whitespace(UChar uchr)
    { return uchr == U' ' || uchr == U'\n' || uchr == U'\t'; }

void run_text_line_tests();

void verify_text_line_content_string(const char * caller, const std::u32string &);

class DefaultCodeModeler final : public CodeModeler {
    void reset_state() override {}
    Response update_model(UStringCIter itr, Cursor) override;
};

// assumes certain optimizations are present, which for supported platforms
// are ok
static_assert(sizeof(DefaultCodeModeler) - sizeof(CodeModeler) == 0, "");

} // end of <anonymous> namespace

// ----------------------------------------------------------------------------

CodeModeler::~CodeModeler() {}

/* static */ CodeModeler & CodeModeler::default_instance() {
    static DefaultCodeModeler instance;
    return instance;
}

// ----------------------------------------------------------------------------

TextLine::TextLine() {}

TextLine::TextLine(const TextLine & rhs):
    m_content(rhs.m_content)
{
    m_image.copy_rendering_details(rhs.m_image);
}

TextLine::TextLine(TextLine && rhs):
    TextLine()
{ swap(rhs); }

/* explicit */ TextLine::TextLine(const std::u32string & content_):
    m_content(content_)
{
    verify_text_line_content_string("TextLine::TextLine", content_);
}

TextLine & TextLine::operator = (const TextLine & rhs) {
    if (&rhs != this) {
        TextLine temp(rhs);
        swap(temp);
    }
    return *this;
}

TextLine & TextLine::operator = (TextLine && rhs) {
    if (&rhs != this) swap(rhs);
    return *this;
}

void TextLine::constrain_to_width(int target_width)
    { m_image.constrain_to_width(target_width); }

void TextLine::set_content(const std::u32string & content_) {
    verify_text_line_content_string("TextLine::set_content", content_);
    m_content = content_;
}

void TextLine::assign_render_options(const RenderOptions & options)
    { m_image.assign_render_options(options); }

void TextLine::set_line_number(int line_number)
    { m_image.set_line_number(line_number); }

void TextLine::assign_default_render_options()
    { assign_render_options(RenderOptions::get_default_instance()); }

TextLine TextLine::split(int column) {
    verify_column_number("TextLine::split", column);
    auto new_line = TextLine(m_content.substr(std::size_t(column)));
    new_line.m_image.copy_rendering_details(m_image);
    m_content.erase(m_content.begin() + column, m_content.end());
    return new_line;
}

int TextLine::push(int column, UChar uchr) {
    verify_column_number("TextLine::push", column);
    if (uchr == TextLines::NEW_LINE) return SPLIT_REQUESTED;
    verify_text("TextLine::push", uchr);
    m_content.insert(m_content.begin() + column, 1, uchr);
    return column + 1;
}

int TextLine::delete_ahead(int column) {
    verify_column_number("TextLine::delete_ahead", column);
    if (column == int(m_content.size())) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column);
    return column;
}

int TextLine::delete_behind(int column) {
    verify_column_number("TextLine::delete_behind", column);
    if (column == 0) return MERGE_REQUESTED;
    m_content.erase(m_content.begin() + column - 1);
    return column - 1;
}

void TextLine::take_contents_of
    (TextLine & other_line, ContentTakingPlacement place)
{
    if (place == PLACE_AT_END) {
        m_content += other_line.content();
    } else {
        assert(place == PLACE_AT_BEGINING);
        m_content.insert(m_content.begin(), other_line.content().begin(),
                         other_line.content().end()                     );
    }
    other_line.wipe(0, other_line.content_length());
}

int TextLine::wipe(int beg, int end) {
    verify_column_number("TextLine::wipe (for beg)", beg);
    verify_column_number("TextLine::wipe (for end)", end);
    m_content.erase(m_content.begin() + beg, m_content.begin() + end);
    return int(m_content.length());
}

void TextLine::copy_characters_from
    (std::u32string & dest, int beg, int end) const
{
    verify_column_number("TextLine::copy_characters_from (for beg)", beg);
    verify_column_number("TextLine::copy_characters_from (for end)", end);
    dest.append(m_content.begin() + beg, m_content.begin() + end);
}

int TextLine::deposit_chatacters_to
    (UStringCIter beg, UStringCIter end, int pos)
{
    verify_column_number("TextLine::deposit_chatacters_to", pos);
    if (beg == end) return pos;
    return deposit_chatacters_to(&*beg, &*(end - 1) + 1, pos);
}

int TextLine::deposit_chatacters_to
    (const UChar * beg, const UChar * end, int pos)
{
    verify_column_number("TextLine::deposit_chatacters_to", pos);
    verify_text("TextLine::deposit_chatacters_to", beg, end);
    if (beg == end) return pos;
    m_content.insert(m_content.begin() + pos, beg, end);
    return pos + int(end - beg);
}

void TextLine::swap(TextLine & other) {
    m_content.swap(other.m_content);
    m_image  .swap(other.m_image  );
}

void TextLine::update_modeler(CodeModeler & modeler)
    { m_image.update_modeler(modeler, m_content); }

int TextLine::height_in_cells() const { return m_image.height_in_cells(); }

const std::u32string & TextLine::content() const { return m_content; }

void TextLine::render_to(TargetTextGrid & target, int offset) const
    { m_image.render_to(target, offset); }

/* static */ void TextLine::run_tests() { run_text_line_tests(); }

/* private */ void TextLine::verify_column_number
    (const char * callername, int column) const
{
    if (column > -1 && column <= int(m_content.size())) return;
    throw std::invalid_argument
        (std::string(callername) + ": given column number is invalid.");
}

/* private */ void TextLine::verify_text
    (const char * callername, UChar uchr) const
{
    if (uchr != TextLines::NEW_LINE && uchr != 0) return;
    throw std::invalid_argument
        (std::string(callername) + ": input characters for TextLine must not "
         "be TextLines::NEW_LINE or the null terminator."                     );
}

/* private */ void TextLine::verify_text
    (const char * callername, const UChar * beg, const UChar * end) const
{
    assert(end >= beg);
    for (auto itr = beg; itr != end; ++itr)
        verify_text(callername, *itr);
}

namespace {

void verify_text_line_content_string(const char * caller, const std::u32string & content) {
    if (content.find(TextLines::NEW_LINE) != std::u32string::npos ||
        content.find(           UChar(0)) != std::u32string::npos   )
    {
        throw std::invalid_argument
            (std::string(caller) + ": content string may not contain a new line.");
    }
}

CodeModeler::Response DefaultCodeModeler::update_model(UStringCIter itr, Cursor) {
    bool is_ws = is_whitespace(*itr);
    for (; *itr && is_ws == is_whitespace(*itr); ++itr) {}
    auto tok_type = is_ws && (*itr == 0 || *itr == TextLines::NEW_LINE) ? LEADING_WHITESPACE : REGULAR_SEQUENCE;
    return Response { itr, tok_type, is_ws };
}

void run_text_line_tests() {
    // Tests will omit the following methods, given that they just pass flow
    // control to the TextLineImage
    // - constrain_to_width
    // - assign_render_options
    // - set_line_number
    // - update_modeler

    {
    TextLine tline(U" ");
    assert(tline.height_in_cells() == 1);
    }
    {
    // hard wrapping
    TextLine tline(U"0123456789");
    tline.constrain_to_width(8);
    tline.update_modeler(CodeModeler::default_instance());
    assert(tline.height_in_cells() == 2);
    }
    {
    // soft wrapping
    TextLine tline(U"function do_something() return \"Hello world\" end");
    tline.constrain_to_width(31);
    tline.update_modeler(CodeModeler::default_instance());
    assert(tline.height_in_cells() == 2);
    }
    // split
    {
    TextLine tline(U"0123456789");
    auto other_tline = tline.split(5);
    tline.update_modeler(CodeModeler::default_instance());
    assert(other_tline.content() == U"56789");
    }
    // split on the beginning
    {
    TextLine tline(U"0123456789");
    auto other_tline = tline.split(0);
    tline.update_modeler(CodeModeler::default_instance());
    assert(other_tline.content() == U"0123456789");
    }
    // take_contents_of
    {
    TextLine tline(U"01234");
    TextLine otline(U"56789");
    tline.take_contents_of(otline, TextLine::PLACE_AT_END);
    for (auto * line : { &tline, &otline })
        line->update_modeler(CodeModeler::default_instance());
    assert(tline.content() == U"0123456789" && otline.content_length() == 0);
    }
    // wipe
    // copy_characters_from
    // deposit_chatacters_to
    {
    NullTextGrid ntg;
    TextLine tline;
    int i = 0;
    tline.push(i++, U'-');
    tline.push(i++, U'-');
    tline.push(i++, TextLines::NEW_LINE);
    tline.split(i - 1);
    ntg.set_width (80);
    ntg.set_height(30);
    tline.constrain_to_width(ntg.width());
    tline.update_modeler(CodeModeler::default_instance());
    tline.render_to(ntg, 3);
    }
    {
    NullTextGrid ntg;
    UserTextSelection uts;
    TextLine tline;
    ntg.set_width(80);
    ntg.set_height(3);
    tline.constrain_to_width(ntg.width());
    int pos = 0;
    for (int i = 0; i != (80 + 79); ++i) {
        pos = tline.push(pos, U'a');
    }
    tline.set_line_number(0);
    tline.update_modeler(CodeModeler::default_instance());
    tline.render_to(ntg, 0);
    }
}

} // end of <anonymous> namespace
