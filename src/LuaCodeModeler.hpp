/****************************************************************************

    File: LuaCodeModeler.hpp
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

#include "TextLine.hpp"

class LuaCodeModeler final : public CodeModeler {
public:
    static constexpr const int REGULAR_CODE       = CodeModeler::REGULAR_SEQUENCE;
    static constexpr const int LEADING_WHITESPACE = CodeModeler::LEADING_WHITESPACE;
    static constexpr const int OPERATOR           = 2;
    static constexpr const int STRING             = 3;
    static constexpr const int COMMENT            = 4;
    static constexpr const int KEYWORD            = 5;
    static constexpr const int NUMERIC            = 6;
    static constexpr const int KEY_CONSTANTS      = 7;
    static constexpr const int UNTERMINATE_STRING = 8;
    static constexpr const int BAD_MULTILINE      = 9;

    LuaCodeModeler();
    void reset_state() override;
    Response update_model(UStringCIter, Cursor) override;
    static ColorPair colors_for_pair(int);
private:
    static constexpr const int NOT_IN_STRING = 0;
    Response handle_multiline(UStringCIter);
    Response handle_comment(UStringCIter);
    Response handle_string(UStringCIter);
    int identify_alphanum(UStringCIter, UStringCIter) const;
    void check_invarients() const;
    Response make_resp
        (UStringCIter next, int token_type, bool always_hardwrap) const
    {
        check_invarients();
        return Response { next, token_type, always_hardwrap };
    }
    Response make_resp(Response passme) const {
        check_invarients();
        return passme;
    }
    bool m_in_comment;
    UChar m_current_string_quote;
    bool m_string_terminates;
    int m_in_multiline_size;
};

template <typename Func>
void for_each_lua_modeler_pair(Func && f) {
    using Lcm = LuaCodeModeler;
    static const auto the_list = {
        Lcm::REGULAR_CODE, Lcm::OPERATOR, Lcm::STRING, Lcm::COMMENT,
        Lcm::KEYWORD, Lcm::NUMERIC, Lcm::LEADING_WHITESPACE, Lcm::KEY_CONSTANTS,
        Lcm::UNTERMINATE_STRING, Lcm::BAD_MULTILINE
    };
    for (auto pair_id : the_list)
        f(pair_id);
}
