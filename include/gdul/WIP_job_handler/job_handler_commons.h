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
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) 
#ifndef GDUL_NOVTABLE
#define GDUL_NOVTABLE __declspec(novtable)
#endif
#else
#define GDUL_NOVTABLE
#endif




namespace gdul
{
namespace job_handler_detail
{
constexpr std::size_t summation(std::size_t from, std::size_t to);

// The number of internal job queues. 
constexpr std::uint8_t Priority_Granularity = 3;

constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 24;

constexpr std::size_t Job_Impl_Chunk_Size(shared_ptr<job_impl, allocator_type>::alloc_size_make_shared());

struct alignas(Job_Impl_Align) Job_Impl_Chunk_Rep { 
	operator uint8_t*()
	{
		return reinterpret_cast<uint8_t*>(this);
	}
	uint8_t dummy[Job_Impl_Chunk_Size]; 
};

using allocator_type = std::allocator<std::uint8_t>;

void set_thread_name(const char* name);

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
}
}