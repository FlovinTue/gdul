// Copyright(c) 2020 Flovin Michaelsen
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

#include <gdul/job_handler/globals.h>

#include <limits>
#include <memory>

#if defined (GDUL_DEBUG)
#include <chrono>
#endif


namespace gdul
{

typedef void* thread_handle;

namespace jh_detail
{

constexpr std::uint32_t Job_Max_Dependencies = std::numeric_limits<std::uint32_t>::max() / 2;

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

void set_thread_name(const char* name, thread_handle handle);
void set_thread_priority(std::int32_t priority, thread_handle handle);
void set_thread_core_affinity(std::uint8_t core, thread_handle handle);
thread_handle create_thread_handle();

#if defined(GDUL_DEBUG)
class timer
{
public:
	timer();

	float get() const;
	void reset();

private:
	std::chrono::high_resolution_clock myClock;
	std::chrono::high_resolution_clock::time_point myFromTime;
};
inline timer::timer()
	: myFromTime(myClock.now())
{}
inline float timer::get() const
{
	return std::chrono::duration_cast<std::chrono::duration<float>>(myClock.now() - myFromTime).count();
}
inline void timer::reset()
{
	myFromTime = myClock.now();
}
#endif
}
}