#include "job_impl.h"
#include <gdul\WIP_job_handler\job_callable_base.h>

namespace gdul {
namespace job_handler_detail {

job_impl::job_impl()
	: myStorage{}
	, myCallable(nullptr)
{
}


job_impl::~job_impl()
{
}

void job_impl::deconstruct(allocator_type & alloc)
{
	myCallable->~callable_base();

	if (myAllocated) {
		alloc.deallocate(myCallableBegin, myAllocated);
	}

	std::uninitialized_fill_n(myStorage, Callable_Max_Size_No_Heap_Alloc, 0);
}

void job_impl::operator()()
{
	(*myCallable)();
}
}
}
