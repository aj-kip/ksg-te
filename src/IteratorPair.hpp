/****************************************************************************

    File: IteratorPair.hpp
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

#include <string>
#include <stdexcept>
#include <iostream>

template <typename IterT>
class IteratorPair {
public:
    using ValueType =
        typename std::remove_const<
        typename std::remove_reference<decltype (*IterT())>::type>::type;
    using IteratorType = IterT;
    IteratorPair() {}
    IteratorPair(IteratorType beg_, IteratorType end_);
    /** @warning Assumes that itr is from the same container as begin and end.
     */
    bool contains(IteratorType itr) const noexcept
        { return begin() <= itr && itr < end(); }
    bool contains(const IteratorPair & pair) const noexcept
        { return begin() >= pair.begin() && pair.end() <= end(); }
    bool is_behind(IteratorType itr) const noexcept
        { return end() <= itr; }
    bool is_ahead(IteratorType itr) const noexcept
        { return itr < begin(); }
    bool is_behind(const IteratorPair & pair) const noexcept
        { return end() <= pair.begin(); }
    bool operator == (const std::basic_string<ValueType> & rhs) const
        { return compare(rhs) == 0; }
    bool operator != (const std::basic_string<ValueType> & rhs) const
        { return compare(rhs) != 0; }
    IteratorType begin() const noexcept;
    IteratorType end() const noexcept;
    IteratorPair<IterT> & set_begin(IteratorType);
    IteratorPair<IterT> & set_end(IteratorType);
private:
    void verify_valid_iterator_range(const char * caller) const;
    int compare(const std::basic_string<ValueType> & str) const {
        auto len = std::size_t(end() - begin());
        return str.compare(0, len, &*begin(), len);
    }
    IteratorType m_begin, m_end;
};

template <typename ValueType>
bool operator == (const std::basic_string<ValueType> & lhs,
                  const IteratorPair<typename std::basic_string<ValueType>::const_iterator> & rhs)
    { return rhs == lhs; }

template <typename ValueType>
bool operator != (const std::basic_string<ValueType> & lhs,
                  const IteratorPair<typename std::basic_string<ValueType>::const_iterator> & rhs)
    { return rhs != lhs; }

template <typename IterT>
IteratorPair<IterT>::IteratorPair
    (IteratorType beg_, IteratorType end_):
    m_begin(beg_), m_end(end_)
{ verify_valid_iterator_range("IteratorPair<IterT>::IteratorPair"); }

template <typename IterT>
typename IteratorPair<IterT>::IteratorType
    IteratorPair<IterT>::begin() const noexcept
    { return m_begin; }

template <typename IterT>
typename IteratorPair<IterT>::IteratorType
    IteratorPair<IterT>::end() const noexcept
    { return m_end; }

template <typename IterT>
IteratorPair<IterT> & IteratorPair<IterT>::set_begin(IteratorType beg_) {
    m_begin = beg_;
    verify_valid_iterator_range("IteratorPair<IterT>::set_begin");
    return *this;
}

template <typename IterT>
IteratorPair<IterT> & IteratorPair<IterT>::set_end(IteratorType end_) {
    m_end = end_;
    verify_valid_iterator_range("IteratorPair<IterT>::set_end");
    return *this;
}

template <typename IterT>
/* private */ void IteratorPair<IterT>::verify_valid_iterator_range
    (const char * caller) const
{
    if (m_begin <= m_end) return;
    throw std::invalid_argument(std::string(caller) + ": beg "
                                "must be equal to or less than end.");
}

template <typename IterT>
std::ostream & operator << (std::ostream & out,
                            const IteratorPair<IterT> & pair)
{
    for (auto itr = pair.begin; itr != pair.end; ++itr)
        out << char(*itr);
    return out;
}

