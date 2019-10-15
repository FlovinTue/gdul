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
