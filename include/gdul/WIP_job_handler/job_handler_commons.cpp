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


#include "job_handler_commons.h"

#if defined(_WIN64) | defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
namespace gdul
{
namespace job_handler_detail
{
#if defined(_WIN64) | defined(_WIN32)
namespace thread_naming
{
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
#endif

#if defined(_WIN64) | defined(_WIN32)
constexpr std::size_t summation(std::size_t from, std::size_t to)
{
	std::size_t sum(from);
	for (std::size_t i = from; i < to; ++i) {
		sum += i + 1;
	}
	return sum;
}
void gdul::job_handler_detail::set_thread_name(const char * name)
{

		thread_naming::THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = name;
		info.dwThreadID = GetCurrentThreadId();
		info.dwFlags = 0;

		__try {
			RaiseException(thread_naming::MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}
#else
void gdul::job_handler_detail::set_thread_name(const char * /*name*/)
{
}
#endif
}
}
