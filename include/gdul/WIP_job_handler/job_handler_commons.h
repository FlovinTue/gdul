// Copyright(c) 2019 Flovin Michaelsen
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

#include <thread>
#include <cstdint>


namespace gdul
{
namespace job_handler_detail
{
// The number of internal job queues. 
constexpr std::uint8_t Priority_Granularity = 3;

constexpr std::uint8_t Default_Job_Priority = 0;
constexpr std::uint8_t Job_Max_Dependencies = 127;
constexpr std::uint8_t Callable_Max_Size_No_Heap_Alloc = 24;

using allocator_type = std::allocator<uint8_t>;

constexpr std::size_t pow2(std::size_t n)
{
	std::size_t sum(1);
	for (std::size_t i = 0; i < n; ++i) {
		sum *= 2;
	}
	return sum;
}
constexpr std::size_t pow2summation(std::size_t from, std::size_t to)
{
	std::size_t sum(from);
	for (std::size_t i = from; i < to; ++i) {
		sum += pow2(i);
	}
	return sum;
}
constexpr std::size_t log2align(std::size_t value)
{
	std::size_t highBit(0);
	std::size_t shift(value);
	for (; shift; ++highBit) {
		shift >>= 1;
	}

	highBit -= static_cast<bool>(highBit);

	const std::size_t mask((std::size_t(1) << (highBit)) - 1);
	const std::size_t remainder((static_cast<bool>(value & mask)));

	const std::size_t sum(highBit + remainder);

	return std::size_t(1) << sum;
}

void set_thread_name(const char* name);

}
}