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

#include <gdul/execution/job_handler/globals.h>

#include <limits>
#include <memory>

#if defined(GDUL_JOB_DEBUG)
#define GDUL_JOB_DEBUG_CONDTIONAL(conditional)conditional;
#else
#define GDUL_JOB_DEBUG_CONDTIONAL(conditional)
#endif

namespace gdul
{
class job_queue;

typedef void* thread_handle;

namespace jh_detail
{
// https://stackoverflow.com/questions/48896142/is-it-possible-to-get-hash-values-as-compile-time-constants
template <typename Str>
constexpr std::size_t constexp_str_hash(const Str& toHash)
{
	std::size_t result = 0xcbf29ce484222325ull;

	for (char c : toHash) {
		result ^= c;
		result *= 1099511628211ull;
	}

	return result;
}

constexpr std::uint32_t Job_Max_Dependencies = std::numeric_limits<std::uint32_t>::max() / 2;
constexpr std::uint32_t Job_Enable_Dependencies = std::numeric_limits<std::uint32_t>::max() - Job_Max_Dependencies;

using allocator_type = std::allocator<uint8_t>;

void set_thread_name(const char* name, thread_handle handle);
void set_thread_priority(std::int32_t priority, thread_handle handle);
thread_handle create_thread_handle();
void close_thread_handle(thread_handle handle);
std::size_t to_batch_size(std::size_t inputSize, job_queue* target);
}
}