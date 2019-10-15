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

