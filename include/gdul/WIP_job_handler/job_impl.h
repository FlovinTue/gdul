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
#include <gdul\WIP_job_handler\callable.h>
#include <gdul\WIP_job_handler\job_impl_allocator.h>

namespace gdul{

class job_handler;
namespace job_handler_detail {

class callable_base;
class alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) job_impl
{
public:
	using job_impl_shared_ptr = shared_ptr<job_impl, job_impl_allocator<std::uint8_t>>;
	using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl, job_impl_allocator<std::uint8_t>>;
	using job_impl_raw_ptr = raw_ptr<job_impl, job_impl_allocator<std::uint8_t>>;

	job_impl() = default;

	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(Callable&& callable, std::uint8_t priority, allocator_type);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(Callable&& callable, std::uint8_t priority, allocator_type alloc);
	~job_impl();
	
	void operator()();

	bool try_attach_child(job_impl_shared_ptr child);

	std::uint8_t get_priority() const;

	void add_dependency();
	void remove_dependencies(std::uint8_t n = 1);

	void enable();
private:

	void attach_sibling(job_impl_shared_ptr sibling);

	void enqueue_children();
	void enqueue_siblings();

	union
	{
		std::uint8_t myStorage[Callable_Max_Size_No_Heap_Alloc];
		struct
		{
			allocator_type myAllocator;
			std::uint8_t* myCallableBegin;
			std::size_t myAllocated;
		}myAllocFields;
	};

	callable_base* myCallable;
	job_handler* myHandler;

	job_impl_atomic_shared_ptr myFirstSibling;
	job_impl_atomic_shared_ptr myFirstChild;

	const std::uint8_t myPriority;

	std::atomic<bool> myFinished;
	std::atomic<std::uint8_t> myDependencies;
};
template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(Callable && callable, std::uint8_t priority, allocator_type alloc)
	: myStorage{}
	, myCallable(nullptr)
	, myFinished(false)
	, myFirstChild(nullptr)
	, myFirstSibling(nullptr)
	, myPriority(priority)
	, myDependencies(Job_Max_Dependencies)
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(myAllocFields)), "too high size / alignment on allocator_type");

	myAllocFields.myAllocator = alloc;

	if (16 < alignof(Callable)) {
		myAllocFields.myAllocated = sizeof(Callable) + alignof(Callable);
	}
	else {
		myAllocFields.myAllocated = sizeof(Callable);
	}
	myAllocFields.myCallableBegin = myAllocFields.myAllocator.allocate(myAllocFields.myAllocated);

	const std::uintptr_t callableBeginAsInt(reinterpret_cast<std::uintptr_t>(myAllocFields.myCallableBegin));
	const std::uintptr_t mod(callableBeginAsInt % alignof(Callable));
	const std::size_t offset(mod ? alignof(Callable) - mod : 0);

	myCallable = new (myAllocFields.myCallableBegin + offset) gdul::job_handler_detail::callable(std::forward<Callable&&>(callable));
}

template<class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(Callable && callable, std::uint8_t priority, allocator_type)
	: myStorage{}
	, myCallable(nullptr)
	, myFinished(false)
	, myFirstChild(nullptr)
	, myFirstSibling(nullptr)
	, myPriority(priority)
	, myDependencies(Job_Max_Dependencies)
{
	myCallable = new (&myStorage[0]) gdul::job_handler_detail::callable(std::forward<Callable&&>(callable));
}
}
}

