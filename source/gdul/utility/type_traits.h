// Copyright(c) 2021 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <limits>
#include <type_traits>
#include <iterator>

namespace gdul {

template <std::size_t Size>
struct least_unsigned_integer
{
	using type =
		std::conditional_t<!(std::numeric_limits<std::uint8_t>::max() < Size), std::uint8_t,
		std::conditional_t<!(std::numeric_limits<uint16_t>::max() < Size), std::uint16_t,
		std::conditional_t<!(std::numeric_limits<std::uint32_t>::max() < Size), std::uint32_t, std::uint64_t>>>;
};

template <std::size_t Size>
using least_unsigned_integer_t = typename least_unsigned_integer<Size>::type;

template <class Iterator>
using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;

template <class T, class = void>
constexpr bool is_iterator_v = false;

template <class T>
constexpr bool is_iterator_v<T, std::void_t<iterator_category<T>>> = true;

template <class T>
struct is_iterator : std::bool_constant<is_iterator_v<T>> {};
}