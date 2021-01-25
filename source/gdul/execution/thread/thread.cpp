#include "thread.h"

#include <cassert>

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

	thread_naming::THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name.c_str();
	info.dwThreadID = GetThreadId(native_handle());
	info.dwFlags = 0;

	__try {
		RaiseException(thread_naming::MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
}
void thread::set_core_affinity(std::uint8_t core)
{
	assert(valid() && "Cannot set affinity to invalid thread");

	const uint8_t core_(core % std::thread::hardware_concurrency());

	const uint64_t affinityMask(1ULL << core);
	while (!SetThreadAffinityMask(native_handle(), affinityMask));
}
void thread::set_execution_priority(std::int32_t priority)
{
	assert(valid() && "Cannot set priority to invalid thread");

	SetThreadPriority(native_handle(), priority);
}
#endif

bool thread::valid() const noexcept
{
	return get_id() != std::thread().get_id();
}
}
