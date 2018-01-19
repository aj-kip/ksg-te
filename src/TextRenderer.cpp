#include "TextRenderer.hpp"
#include "TextGrid.hpp"

#include <limits>

#include <cassert>

class TextWriterDefs {
    friend class TextRenderer;
    static constexpr const int MAX_VALUE = 100;
    using GridWriter = TextRendererPriv::GridWriter;
    class TextGridWriter final : public GridWriter {
    public:
        TextGridWriter(): write_position(0, 0), parent_grid(nullptr) {}
        void write(sf::Color fore, sf::Color back, Cursor, const std::u32string &) override;
        void write(sf::Color fore, sf::Color back, Cursor, UChar) override;
        int width() const override;
        int height() const override;

        Cursor write_position;
        TextGrid * parent_grid;
    private:
        void verify_parent_present(const char * funcname) const;
    };
    class SimpleString final : public GridWriter {
    public:
        ~SimpleString() override;
        void write
            (sf::Color, sf::Color, Cursor, const std::u32string & str) override
            { product_string += str; }
        void write(sf::Color, sf::Color, Cursor, UChar c) override
            { product_string += c; }
        int width () const override { return MAX_VALUE; }
        int height() const override { return MAX_VALUE; }
        std::u32string product_string;
    };
    class HighlightTracker final : public GridWriter {
    public:
        HighlightTracker(): highlighted_count(0), non_highlighted_count(0) {}
        void write
            (sf::Color, sf::Color, Cursor, const std::u32string &) override;
        void write(sf::Color, sf::Color, Cursor, UChar c) override;
        int width () const override { return MAX_VALUE; }
        int height() const override { return MAX_VALUE; }
        int highlighted_count;
        int non_highlighted_count;
    };
    class MultiLine final : public GridWriter {
    public:
        static constexpr const int LINE_MAX = 80;
        void write
            (sf::Color, sf::Color, Cursor, const std::u32string &) override;
        void write(sf::Color, sf::Color, Cursor, UChar c) override;
        int width () const override { return LINE_MAX ; }
        int height() const override { return MAX_VALUE; }
        void adjust_size_for(Cursor);
        std::vector<std::u32string> lines;
        Cursor last_cursor_of_string_write;
    private:
        void check_invarients() const;
    };
    class FillTracker final : public GridWriter {
    public:
        static constexpr const int MAX_DIM = 80;
        void write
            (sf::Color, sf::Color, Cursor, const std::u32string &) override;
        void write(sf::Color, sf::Color, Cursor, UChar c) override;
        int width () const override { return MAX_DIM; }
        int height() const override { return MAX_DIM; }
        bool filled() const;
    private:
        void hit_cell(Cursor);
        std::vector<bool> m_hit_cells;
    };
};

namespace {

void push_string_to(const std::u32string &, TextLines *);

} // end of <anonymous> namespace

/* static */ const sf::Color TextRenderer::default_keyword_fore_c = sf::Color(200, 200, 0);
/* static */ const sf::Color TextRenderer::default_keyword_back_c;
/* static */ const sf::Color TextRenderer::default_fore_c = sf::Color::White;
/* static */ const sf::Color TextRenderer::default_back_c = sf::Color(12, 12, 12);
/* static */ const sf::Color TextRenderer::default_select_fore_c;
/* static */ const sf::Color TextRenderer::default_select_back_c;

TextRendererPriv::GridWriter::~GridWriter() {}

Cursor TextRendererPriv::GridWriter::next_cursor(Cursor cur) const {
    verify_cursor_validity("HighlighterPriv::GridWriter::next_cursor", cur);
    ++cur.column;
    if (cur.column == width()) {
        ++cur.line;
        cur.column = 0;
    }
    return cur;
}

Cursor TextRendererPriv::GridWriter::end_cursor() const
    { return Cursor(height(), 0); }

/* protected */ void TextRendererPriv::GridWriter::verify_cursor_validity
    (const char * funcname, Cursor cur) const
{
    if (cur.column < width() && cur.line < height()) return;
    throw std::out_of_range(std::string(funcname) +
                            ": invalid cursor falls outside of grid.");
}

TextRenderer::TextRenderer() {}

void TextRenderer::setup_defaults() {
    m_info.default_back = default_back_c;
    m_info.default_fore = default_fore_c;
    m_info.keyword_fore = default_keyword_fore_c;
    m_info.select_back  = default_select_back_c;
    m_info.select_fore  = default_select_fore_c;
}

void TextRenderer::render_lines_to
    (TextGrid * text_grid, Cursor starting_point, const TextLines & tlines)
{
    render_lines_to(text_grid, starting_point, tlines, UserTextSelection());
}

void TextRenderer::render_lines_to
    (TextGrid * text_grid, Cursor starting_point, const TextLines & tlines,
     const UserTextSelection & selection)
{
    TextWriterDefs::TextGridWriter writer;
    writer.parent_grid = text_grid;
    writer.write_position = starting_point;
    render_lines_to(writer, starting_point, tlines, selection);
}

/* private */ void TextRenderer::render_lines_to
    (GridWriter & writer, Cursor tline_pos, const TextLines & tlines,
     const UserTextSelection & selection)
{
    Cursor grid_pos(0, 0);
    std::u32string buff;
    const auto height = writer.height();
    auto old_tlines_pos = tline_pos;
    while (grid_pos.line <= height) {
        if (tline_pos == tlines.end_cursor()) {
            if (!buff.empty()) {
                grid_pos = render_word_to(writer, grid_pos, buff);
            }
            fill_remainder_with_blanks(writer, grid_pos);
            return;
        }
        auto uchr = tlines.read_character(tline_pos);
        // test here is essentially: at point of word breaking
        if (is_whitespace(uchr)) {
            if (!buff.empty()) {
                grid_pos = render_word_to(writer, grid_pos, buff);
            }
            buff.clear();
            auto fore = m_info.default_fore;
            auto back = m_info.default_back;
            if (selection.is_in_range(tline_pos)) {
                fore = invert(fore);
                back = invert(back);
            }
            writer.write(fore, back, grid_pos, uchr);
            if (uchr == TextLines::NEW_LINE) {
                auto old_line = grid_pos.line;
                while (old_line == grid_pos.line) {
                    writer.write(m_info.default_fore, m_info.default_back, grid_pos, U' ');
                    grid_pos = writer.next_cursor(grid_pos);
                }
            } else {
                grid_pos = writer.next_cursor(grid_pos);
            }
        } else {
            buff += uchr;
        }
        tline_pos = tlines.next_cursor(tline_pos);
        assert(tline_pos != old_tlines_pos);
        old_tlines_pos = tline_pos;
    }
}

/* private */ Cursor TextRenderer::render_word_to
    (GridWriter & writer, Cursor grid_pos, const std::u32string & buff,
     const UserTextSelection &)
{
    const auto width  = writer.width ();
    const auto height = writer.height();
    auto fore_color = m_info.default_fore;
    auto grid_pos_is_valid = [&grid_pos, &writer] () {
        return grid_pos.line <= writer.height() &&
               grid_pos.column < writer.width();
    };
    if (m_info.keywords.find(buff) != m_info.keywords.end())
        fore_color = m_info.keyword_fore;
    assert(grid_pos_is_valid());
    if (int(buff.size()) >= width) {
        // hard wrap
        // not enough space for an entire grid line
        std::u32string temp;
        auto itr = buff.begin();
        while (buff.end() - itr >= width) {
            temp.insert(temp.begin(), itr, itr + width);
            writer.write(fore_color, m_info.default_back, grid_pos, temp);
            temp.clear();
            ++grid_pos.line;
            if (grid_pos.line == height) {
                assert(grid_pos_is_valid());
                return grid_pos;
            }
            grid_pos.column = 0;
            itr += width;
        }
        // write the rest
        temp.insert(temp.begin(), itr, buff.end());
        writer.write(fore_color, m_info.default_back, grid_pos, temp);
        grid_pos.column += (buff.end() - itr);
    } else if (grid_pos.column + int(buff.size()) >= width) {
        // soft wrap
        // fill line with blanks, procede and write to next line
        // unless out of space
        while (grid_pos.column != width) {
            writer.write(m_info.default_fore, m_info.default_back, grid_pos, U' ');
            ++grid_pos.column;
        }
        ++grid_pos.line;
        if (grid_pos.line == height) {
            assert(grid_pos_is_valid());
            return grid_pos;
        }
        grid_pos.column = 0;
        writer.write(fore_color, m_info.default_back, grid_pos, buff);
        grid_pos.column = int(buff.size());
    } else {
        // normal write
        writer.write(fore_color, m_info.default_back, grid_pos, buff);
        grid_pos.column += int(buff.size());
    }
    assert(grid_pos_is_valid());
    return grid_pos;
}

void TextRenderer::fill_remainder_with_blanks
    (GridWriter & writer, Cursor grid_pos)
{
    while (grid_pos != writer.end_cursor()) {
        writer.write(m_info.default_fore, m_info.default_back, grid_pos, U' ');
        grid_pos = writer.next_cursor(grid_pos);
    }
}

void TextRenderer::add_keyword(const std::u32string & keyword) {
    m_info.keywords.insert(keyword);
}

/* static */ void TextRenderer::run_tests() {
    using TestWriter = TextWriterDefs;
    // 8 test cases
    assert(is_whitespace(TextLines::NEW_LINE));
    // 1. simply render a single string (word)
    {
    static constexpr const auto * const TEST_TEXT = U"hello there";
    TextLines tlines;
    TextRenderer hltr;
    TestWriter::SimpleString writer;
    hltr.setup_defaults();
    push_string_to(TEST_TEXT, &tlines);
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.product_string.substr(0, std::u32string(TEST_TEXT).length()) == TEST_TEXT);
    }
    // 2. highlight keywords
    {
    TextLines tlines;
    push_string_to(U"function do_something() end", &tlines);
    TestWriter::HighlightTracker writer;
    TextRenderer hltr;
    hltr.add_keyword(U"function");
    hltr.add_keyword(U"end");
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.highlighted_count == 2 &&
           writer.non_highlighted_count == 1);
    }
    // 3. line wrapping
    {
    TextLines tlines;
    push_string_to(U"There are so many things in the world which both confuse "
                   "and amaze me. I'm not quite sure how to handle all of it, "
                   "but I'll do the best that I can.", &tlines);
    TestWriter::MultiLine writer;
    TextRenderer hltr;
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.lines.size() > 1 &&
           writer.last_cursor_of_string_write.line > 0);
    }
    // 4. regular multi line
    auto first_part_match = []
        (const std::u32string & first, const std::u32string & match)
    { return match.substr(0, first.length()) == first; };
    {
    TextLines tlines;
    TestWriter::MultiLine writer;
    TextRenderer hltr;
    push_string_to(U"banana\napple\norange", &tlines);
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.lines[3][0] == U' ');
    assert(first_part_match(U"banana ", writer.lines[0]));
    assert(first_part_match(U"apple " , writer.lines[1]));
    assert(first_part_match(U"orange ", writer.lines[2]));
    }
    // 5. different starting line (start on line one, rather than zero)
    {
    static_assert(TextLines::NEW_LINE == U'\n', "Test case needs an upgrade.");
    TextLines tlines;
    push_string_to(U"apple\nbanana\npear", &tlines);
    TestWriter::MultiLine writer;
    TextRenderer hltr;
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(1, 0), tlines);
    assert(writer.lines[2][0] == U' ');
    assert(first_part_match(U"banana", writer.lines[0]));
    assert(first_part_match(U"pear"  , writer.lines[1]));
    }
    // 6. line wrap large continuous non-whitespace string (80+ chars)
    {
    static_assert(TestWriter::MultiLine::LINE_MAX == 80, "Case 5 needs rewriting.");
    TextLines tlines;
    push_string_to(U"abcdefghijklmnopqrst""abcdefghijklmnopqrst"
                   "abcdefghijklmnopqrst""abcdefghijklmnopqrst"
                   "1"/* 81 characters exactly */, &tlines);
    TextRenderer hltr;
    TestWriter::MultiLine writer;
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.lines[2][0] == U' ');
    assert(writer.lines[0].size() == TestWriter::MultiLine::LINE_MAX &&
           writer.lines[1][0] == U'1');
    }
    // 7. all cells must be filled!
    {
    TextLines tlines;
    push_string_to(U"I could copy filler text of questionable legality for "
                   "use, or I could simply write my own filler text.\nI've "
                   "decided not to try, and use my own instead. I will be as "
                   "needlessly verbose as I can for the purposes of testing "
                   "this program.", &tlines);
    TextRenderer hltr;
    TestWriter::FillTracker writer;
    hltr.setup_defaults();
    hltr.render_lines_to(writer, Cursor(0, 0), tlines);
    assert(writer.filled());
    }
    // 8. handle tab characters
    // 9. handle cursor mapping
}

/* static */ bool TextRenderer::is_whitespace(UChar c) {
    return c == U' ' || c == U'\t' || c == U'\r' || c == U'\n';
}

/* static */ sf::Color TextRenderer::invert(sf::Color color) {
    return sf::Color(255 - color.r, 255 - color.g, 255 - color.b, color.a);
}


// ----------------------------------------------------------------------------

namespace {

void push_string_to(const std::u32string & ustr, TextLines * tlines) {
    Cursor cursor(0, 0);
    for (auto uc : ustr) {
        cursor = tlines->push(cursor, uc);
    }
}

} // end of <anonymous> namespace

void TextWriterDefs::TextGridWriter::write
    (sf::Color fore, sf::Color back, Cursor cur, const std::u32string & ustr)
{
    // ensure behavior guarentee
    assert(parent_grid);
    assert(cur.column + int(ustr.size()) < width());
    verify_cursor_validity("HighlighterWriterDefs::TextGridWriter::write", cur);
    verify_parent_present("HighlighterWriterDefs::TextGridWriter::write");
    Cursor end_cursor(cur.line, cur.column + int(ustr.size()));
    auto ustr_itr = ustr.begin();
    for (; cur != end_cursor; ++cur.column) {
        assert(ustr_itr != ustr.end());
        parent_grid->set_cell(cur, fore, back, *ustr_itr);
        ++ustr_itr;
    }
}

void TextWriterDefs::TextGridWriter::write
    (sf::Color fore, sf::Color back, Cursor cur, UChar uc)
{
    verify_cursor_validity("HighlighterWriterDefs::TextGridWriter::write", cur);
    verify_parent_present("HighlighterWriterDefs::TextGridWriter::write");
    parent_grid->set_cell(cur, fore, back, uc);
}

int TextWriterDefs::TextGridWriter::width() const {
    return parent_grid->width_in_cells();
}

int TextWriterDefs::TextGridWriter::height() const {
    return parent_grid->height_in_cells();
}

void TextWriterDefs::TextGridWriter::verify_parent_present
    (const char * funcname) const
{
    if (parent_grid) return;
    throw std::runtime_error(std::string(funcname) + ": parent grid is not "
                             "set, which is required for this function.");
}

// ----------------------------------------------------------------------------

TextWriterDefs::SimpleString::~SimpleString() {}

// ----------------------------------------------------------------------------

void TextWriterDefs::HighlightTracker::write
    (sf::Color fore, sf::Color, Cursor cur, const std::u32string &)
{
    verify_cursor_validity("HighlighterWriterDefs::HighlightTracker::write", cur);
    if (fore == TextRenderer::default_keyword_fore_c)
        ++highlighted_count;
    else
        ++non_highlighted_count;
}

void TextWriterDefs::HighlightTracker::write
    (sf::Color, sf::Color, Cursor cur, UChar)
{ verify_cursor_validity("HighlighterWriterDefs::HighlightTracker::write", cur); }

// ----------------------------------------------------------------------------

void TextWriterDefs::MultiLine::write
    (sf::Color, sf::Color, Cursor cursor, const std::u32string & buff)
{
    check_invarients();
    verify_cursor_validity("HighlighterWriterDefs::MultiLine::write", cursor);
    adjust_size_for(cursor);
    std::string temp;
    for (auto uchr : buff) {
        if (TextLines::is_ascii(uchr))
            temp += char(uchr & 0x7F);
        else
            temp += '?';
    }

    last_cursor_of_string_write = cursor;
    *(lines.begin() + cursor.line) += buff;
    check_invarients();
}

void TextWriterDefs::MultiLine::write
    (sf::Color, sf::Color, Cursor cursor, UChar c)
{
    check_invarients();
    verify_cursor_validity("HighlighterWriterDefs::MultiLine::write", cursor);
    adjust_size_for(cursor);

    if (c != TextLines::NEW_LINE) {
        *(lines.begin() + cursor.line) += c;
    }
    check_invarients();
}

void TextWriterDefs::MultiLine::adjust_size_for(Cursor cur) {
    // resize: no information on preserving value of previous lines
    while (int(lines.size()) <= cur.line) {
        lines.emplace_back();
    }
}

/* private */ void TextWriterDefs::MultiLine::check_invarients() const {
    for (const auto & line : lines) {
        assert(line.size() <= LINE_MAX);
    }
}

// ----------------------------------------------------------------------------

void TextWriterDefs::FillTracker::write
    (sf::Color, sf::Color, Cursor cur, const std::u32string & buff)
{
    verify_cursor_validity("HighlighterWriterDefs::FillTracker::write", cur);
    if (m_hit_cells.empty())
        m_hit_cells.resize(std::size_t(width()*height()), false);
    int end_col = cur.column + int(buff.size());
    for (; cur.column != end_col; ++cur.column) {
        hit_cell(cur);
    }
}

void TextWriterDefs::FillTracker::write
    (sf::Color, sf::Color, Cursor cur, UChar uchr)
{
    verify_cursor_validity("HighlighterWriterDefs::FillTracker::write", cur);
    if (uchr == TextLines::NEW_LINE) return;
    if (m_hit_cells.empty())
        m_hit_cells.resize(std::size_t(width()*height()), false);
    hit_cell(cur);
}

bool TextWriterDefs::FillTracker::filled() const {
    for (bool is_hit : m_hit_cells) {
        if (!is_hit) return false;
    }
    return true;
}

/* private */ void TextWriterDefs::FillTracker::hit_cell(Cursor cur) {
    auto index = std::size_t(cur.column + cur.line*width());
    if (m_hit_cells[index]) {
        throw std::runtime_error(
            "HighlighterWriterDefs::FillTracker::hit_cell: a cell should "
            "be hit exactly once, not more, by the rendering algorithm."  );
    }
    m_hit_cells[index] = true;
}
