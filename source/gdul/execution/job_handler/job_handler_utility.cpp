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

#include <thread>

#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/execution/job_handler/job_queue.h>

#if defined(_WIN64) | defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
namespace gdul
{
namespace jh_detail
{
#if defined(_WIN64) | defined(_WIN32)

#endif

#if defined(_WIN64) | defined(_WIN32)
thread_handle create_thread_handle()
{
	thread_handle handle = 0;
	DuplicateHandle(GetCurrentProcess(),
		GetCurrentThread(),
		GetCurrentProcess(),
		&handle,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS);

	return handle;
}
void close_thread_handle(thread_handle handle)
{
	CloseHandle(handle);
}
std::size_t to_batch_size(std::size_t inputSize, job_queue* target)
{
	const std::uint8_t queueAssignees(target->m_assignees.load(std::memory_order_relaxed));
	const std::uint8_t div(queueAssignees ? queueAssignees : 1);
	const std::uint8_t offset(div * 2);

	const std::size_t desiredSize(inputSize / offset);

	return desiredSize ? desiredSize : 1;
}
#else
void set_thread_name(const char*)
{}
void set_thread_priority(std::uint8_t, thread_handle)
{}
void set_thread_core_affinity(std::uint8_t, thread_handle)
{}
thread_handle create_thread_handle()
{
	return nullptr;
}
void close_thread_handle(thread_handle)
{}
#endif

}
}
