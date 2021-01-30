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