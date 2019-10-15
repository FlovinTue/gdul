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
