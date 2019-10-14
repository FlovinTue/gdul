#include "job_impl.h"
#include <gdul\WIP_job_handler\callable_base.h>

namespace gdul {
namespace job_handler_detail {


job_impl::~job_impl()
{
	myCallable->~callable_base();

	if ((void*)myCallable != (void*)&myStorage[0]) {
		myAllocatedFields.myAllocator.deallocate(myAllocatedFields.myCallableBegin, myAllocatedFields.myAllocated);
	}

	std::uninitialized_fill_n(myStorage, Callable_Max_Size_No_Heap_Alloc, 0);
}

void job_impl::operator()()
{
	(*myCallable)();
}
}
}
