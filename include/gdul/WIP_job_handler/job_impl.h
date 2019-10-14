#pragma once

#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul{
namespace job_handler_detail {

constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 32;

class callable_base;
class alignas(Callable_Max_Size_No_Heap_Alloc) job_impl
{
public:
	job_impl();
	~job_impl();

	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct_callable(Callable&& callable, allocator_type);
	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct_callable(const Callable& callable, allocator_type);

	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct_callable(Callable&& callable, allocator_type alloc);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct_callable(const Callable& callable, allocator_type alloc);

	void deconstruct_callable(allocator_type& alloc);

	void operator()();
private:
	union
	{
		std::uint8_t myStorage[Callable_Max_Size_No_Heap_Alloc];
		struct
		{
			std::uint8_t* myCallableBegin;
			std::size_t myAllocated;
		};
	};
	callable_base* myCallable;
};
}
}

