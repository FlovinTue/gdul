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
#include <gdul\WIP_job_handler\job_impl_allocator.h>

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
using allocator_type = std::allocator<std::uint8_t>;
using job_impl_shared_ptr = shared_ptr<job_handler_detail::job_impl, job_handler_detail::job_impl_allocator<uint8_t>>;
using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl, job_impl_allocator<uint8_t>>;
using job_impl_raw_ptr = raw_ptr<job_impl, job_impl_allocator<uint8_t>>;


// The number of internal job queues. 
constexpr std::uint8_t Priority_Granularity = 3;
constexpr std::uint8_t Default_Job_Priority = 0;
constexpr std::uint8_t Job_Max_Dependencies = 127;
constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 24;

constexpr std::size_t Job_Impl_Chunk_Size(shared_ptr<job_impl, allocator_type>::alloc_size_make_shared());

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
struct alignas(Job_Impl_Align) job_impl_chunk_rep { 
	operator uint8_t*()
	{
		return reinterpret_cast<uint8_t*>(this);
	}
	uint8_t dummy[Job_Impl_Chunk_Size]; 
};


void set_thread_name(const char* name);

}
}