//Copyright(c) 2021 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <cstdint>
#include <limits>
#include <algorithm>

namespace gdul {

constexpr bool is_prime(std::size_t value)
{
	if (value < 2) {
		return false;
	}
	if (value < 4) {
		return true;
	}

	const std::uint8_t mod6(value % 6);

	if (mod6 != 1 && mod6 != 5) {
		return false;
	}

	for (std::size_t div(3); !(value < div * div); ++div) {
		if (value % div == 0) {
			return false;
		}
	}

	return true;
}

constexpr std::size_t align_value_pow2(std::size_t from)
{
	const std::size_t _from(from + (std::size_t)!(bool)(from));
	std::size_t shift(_from - 1);
	std::uint8_t lastBit(0);

	for (; shift; lastBit++) {
		shift >>= 1;
	}

	return std::size_t(1ull << lastBit);
}

constexpr std::size_t align_value_pow2(std::size_t from, std::size_t max)
{
	return std::min<std::size_t>(align_value_pow2(from), max);
}

constexpr std::size_t align_value(std::size_t value, std::size_t align)
{
	const std::size_t mod(value % align);
	const std::size_t multi(static_cast<bool>(mod));
	const std::size_t diff(align - mod);
	const std::size_t offset(diff * multi);

	return value + offset;
}

constexpr std::size_t align_value_prime(std::size_t value)
{
	if (value < 3) {
		return 2;
	}

	if (is_prime(value)) {
		return value;
	}

	if (is_prime(value + 1)) {
		return value + 1;
	}

	std::size_t probe(align_value(value + 1, 6));

	for (;;probe += 6) {
		if (is_prime(probe - 1)) {
			return probe - 1;
		}
		if (is_prime(probe + 1)) {
			return probe + 1;
		}
	}

	return probe;
}
}