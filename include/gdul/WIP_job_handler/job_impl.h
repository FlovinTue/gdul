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

#pragma once

#include <gdul\WIP_job_handler\job_handler_commons.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul{
namespace job_handler_detail {

class callable_base;
class alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) job_impl
{
public:
	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(Callable&& callable, allocator_type);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(Callable&& callable, allocator_type alloc);
	~job_impl();

	void operator()();
private:

	union
	{
		std::uint8_t myStorage[Callable_Max_Size_No_Heap_Alloc];
		struct
		{
			allocator_type myAllocator;
			std::uint8_t* myCallableBegin;
			std::size_t myAllocated;
		}myAllocatedFields;
	};

	callable_base* myCallable;

	atomic_shared_ptr<job_impl, job_impl_allocator<uint8_t>> myFirstChild;
};
template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(Callable && callable, allocator_type alloc)
	: myStorage{}
	, myCallable(nullptr)
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(myAllocatedFields)), "too high size / alignment on allocator_type");

	myAllocatedFields.myAllocator = alloc;

	if (16 < align) {
		myAllocatedFields.myAllocated = sizeof(Callable) + alignof(Callable);
	}
	else {
		myAllocatedFields.myAllocated = sizeof(Callable);
	}
	myAllocatedFields.myCallableBegin = myAllocatedFields.myAllocator.allocate(myAllocatedFields.myAllocated);

	const std::uintptr_t callableBeginAsInt(reinterpret_cast<std::uintptr_t>(myCallableBegin));
	const std::uintptr_t mod(callableBeginAsInt % alignof(Callable));
	const std::size_t offset(mod ? align - mod : 0);

	new (myAllocatedFields.myCallableBegin + offset) Callable(std::forward<Callable&&>(callable));
}

template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(Callable && callable, allocator_type)
	: myStorage{}
	, myCallable(nullptr)
{
	new (&myStorage[0]) Callable(std::forward<Callable&&>(callable));
}
}
}

