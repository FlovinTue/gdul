#pragma once

#if  defined(_MSC_VER) && !defined(__INTEL_COMPILER) && !defined(GDUL_NOVTABLE)
#define GDUL_NOVTABLE __declspec(novtable)
#else
#define GDUL_NOVTABLE
#endif


namespace gdul {
namespace job_handler_detail {
GDUL_NOVTABLE class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;

	virtual void dealloc() = 0;
};
}
}
