/****************************************************************************

    File: TextLine.hpp
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

#include "Cursor.hpp"
#include "TargetTextGrid.hpp"
#include "IteratorPair.hpp"

#include <common/MultiType.hpp>

#include <vector>
#include <string>
#include <stdexcept>
#include <memory>

#pragma once

// multi line "objects" make this especially difficult...
// namely C's multiline comments, Lua's multiline strings
struct CodeModeler {
    using UStringCIter = std::u32string::const_iterator;
    using UStringIteratorPair = IteratorPair<UStringCIter>;
    struct Response {
        UStringCIter next;
        int token_type;
        bool always_hardwrap;
    };
    // token type's returned by the default instance
    static constexpr const int REGULAR_SEQUENCE   = 0;
    static constexpr const int LEADING_WHITESPACE = 1;
    /** @note Puesdo-writable (and stateless) object */
    static CodeModeler & default_instance();
    virtual ~CodeModeler();
    virtual void reset_state() = 0;
    virtual Response update_model(UStringCIter, Cursor) = 0;
};

// subject
class RangesUpdater {
public:
    RangesUpdater(): m_observer(this) {}
    RangesUpdater(const RangesUpdater &);
    RangesUpdater & operator = (const RangesUpdater &);
    virtual ~RangesUpdater();
    void update_ranges(CodeModeler & = CodeModeler::default_instance());
    void update_ranges_skip_redirection(CodeModeler & = CodeModeler::default_instance());
    void assign_ranges_updater(RangesUpdater &);
    void reset_ranges_updater_to_this();
protected:
    virtual void update_ranges_impl(CodeModeler &) = 0;
private:
    RangesUpdater * m_observer;
};

// content string + extra ending space
class TextLine final : public RangesUpdater {
public:
    enum ContentTakingPlacement { PLACE_AT_BEGINING, PLACE_AT_END };
    static constexpr const int MERGE_REQUESTED = -1;
    static constexpr const int SPLIT_REQUESTED = -1;
    static constexpr const int NO_LINE_NUMBER  = -1;
    using UStringCIter = std::u32string::const_iterator;
    using UStrIteratorPair = IteratorPair<UStringCIter>;
    TextLine();
    TextLine(const TextLine &);
    TextLine(TextLine &&);
    explicit TextLine(const std::u32string &);

    TextLine & operator = (const TextLine &);
    TextLine & operator = (TextLine &&);

    // -------------------------- TextLine Settings ---------------------------

    void constrain_to_width(int);
    void set_content(const std::u32string &);
    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_render_options(const RenderOptions &);
    void set_line_number(int);
    /** @warning Does not in anyway maintain ownership over the given object
     *           reference. Given object must survive the life of this object.
     */
    void assign_code_modeler(CodeModeler &);

    void assign_default_render_options();

    // ------------------------ whole content editing -------------------------

    TextLine split(int column);
    void take_contents_of(TextLine &, decltype(PLACE_AT_BEGINING));
    int wipe(int beg, int end);
    void copy_characters_from(std::u32string &, int beg, int end) const;
    int deposit_chatacters_to
        (UStringCIter beg, UStringCIter end, int pos);
    int deposit_chatacters_to(const UChar * beg, const UChar * end, int pos);

    void swap(TextLine &);

    // ----------------------- single character editing -----------------------

    // may return SPLIT_REQUESTED
    int push(int column, UChar);
    // may return MERGE_REQUESTED
    int delete_ahead(int column);
    // may return MERGE_REQUESTED
    int delete_behind(int column);

    // ------------------------------ accessors -------------------------------

    int recorded_grid_width() const;
    int height_in_cells() const;
    const std::u32string & content() const;
    int content_length() const { return int(content().length()); }

    void render_to(TargetTextGrid &, int offset) const;

    static void run_tests();
private:
    using IteratorPairCIterator = std::vector<UStrIteratorPair>::const_iterator;
    struct TokenInfo {
        TokenInfo(): type(0) {}
        TokenInfo(int type_, UStrIteratorPair pair_): type(type_), pair(pair_) {}
        TokenInfo(int type_, UStringCIter beg, UStringCIter end):
            type(type_), pair(beg, end)
        {}
        int type;
        UStrIteratorPair pair;
    };
    using TokenInfoCIter = std::vector<TokenInfo>::const_iterator;
    void verify_column_number(const char * callername, int) const;
    void verify_text(const char * callername, UChar) const;
    void verify_text(const char * callername, const UChar *, const UChar *) const;
    void update_ranges_impl(CodeModeler &) override;
    /**
     * @param target
     * @param offset row to render content to
     * @param word_itr first "word" to render, providing
     * @param row_end
     * @param line_number
     * @return Word on next row to render OR end iterator
     */
    TokenInfoCIter render_row
        (TargetTextGrid & target, int offset, TokenInfoCIter word_itr,
         UStringCIter row_end) const;
    void fill_row_with_blanks(TargetTextGrid &, Cursor write_pos) const;
    void render_end_space(TargetTextGrid &, int offset) const;
    void check_invarients() const;

    int m_grid_width;
    // required for edge case were all cells on the last row are occupied by
    // content, and therefore needing an extra space on the next so that the
    // user can type text at the end of the line
    int m_extra_end_space;
    // does not contain iterators begin and end in m_content
    std::vector<UStringCIter> m_row_ranges;
    // invarient -> cannot cross line range iterators
    //std::vector<UStrIteratorPair> m_word_ranges;

    std::u32string m_content;
    const RenderOptions * m_rendering_options;

    CodeModeler * m_code_modeler;
    int m_line_number;

    std::vector<TokenInfo> m_tokens;
};
