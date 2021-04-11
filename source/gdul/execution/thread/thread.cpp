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

#include "thread.h"

#include <cassert>
#include <string>

#if defined(_WIN64) | defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul {

#if defined(_WIN64) | defined(_WIN32)
namespace thread_naming {
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)  
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType;
	LPCSTR szName;
	DWORD dwThreadID;
	DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)  
};

void thread::set_name(const std::string& name)
{
	assert(valid() && "Cannot set name to invalid thread");

	set_name(name, native_handle());
}
void thread::set_core_affinity(std::uint8_t core)
{
	assert(valid() && "Cannot set affinity to invalid thread");

	set_core_affinity(core, native_handle());
}
void thread::set_execution_priority(std::int32_t priority)
{
	assert(valid() && "Cannot set priority to invalid thread");

	set_execution_priority(priority, this->native_handle());
}
void thread::set_name(const std::string& name, void* handle)
{
	thread_naming::THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = GetThreadId(handle);
	info.dwFlags = 0;

	__try {
		RaiseException(thread_naming::MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
}
void thread::set_core_affinity(std::uint8_t core, void* handle)
{
	const uint8_t core_(core % std::thread::hardware_concurrency());

	const uint64_t affinityMask(1ULL << core);
	while (!SetThreadAffinityMask(handle, affinityMask));
}
void thread::set_execution_priority(std::int32_t priority, void* handle)
{
	SetThreadPriority(handle, priority);
}
#endif

bool thread::valid() const noexcept
{
	return get_id() != std::thread().get_id();
}
}

#if defined(_WIN64) | defined(_WIN32)
void std::this_thread::set_name(const std::string& name)
{
	gdul::thread::set_name(name, GetCurrentThread());
}

void std::this_thread::set_core_affinity(std::uint8_t core)
{
	gdul::thread::set_core_affinity(core, GetCurrentThread());
}

void std::this_thread::set_execution_priority(std::int32_t priority)
{
	gdul::thread::set_execution_priority(priority, GetCurrentThread());
}

#else
void std::this_thread::set_name(const std::string&)
{
	assert(false && "Not implemented")
}

void std::this_thread::set_core_affinity(std::uint8_t)
{
	assert(false && "Not implemented")
}

void std::this_thread::set_execution_priority(std::uint8_t)
{
	assert(false && "Not implemented")
}
#endif
bool std::this_thread::valid() noexcept
{
	return get_id() != std::thread().get_id();
}