#pragma once

#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul{
namespace job_handler_detail {

constexpr std::size_t log2align(std::size_t value)
{
	std::size_t highBit(0);
	std::size_t shift(value);
	for (; shift; ++highBit) {
		shift >>= 1;
	}

	highBit -= static_cast<bool>(highBit);

	const std::size_t mask((std::size_t(1) << (highBit)) - 1);
	const std::size_t remainder((static_cast<bool>(value & mask)));

	const std::size_t sum(highBit + remainder);

	return std::size_t(1) << sum;
}

constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 24;
constexpr std::size_t Job_Impl_Align(log2align(Callable_Max_Size_No_Heap_Alloc));

class callable_base;
class alignas(Job_Impl_Align) job_impl
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

