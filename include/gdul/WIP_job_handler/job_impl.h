#pragma once


namespace gdul{
namespace job_handler_detail {

constexpr std::size_t Callable_Max_Size_No_Heap_Alloc = 32;

template <class Callable>
constexpr bool callable_fit_test();

class job_callable_base;
class alignas(Callable_Max_Size_No_Heap_Alloc) job_impl
{
public:
	job_impl();
	~job_impl();

	template <class Callable, class Allocator, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct(Callable&& callable, Allocator);
	template <class Callable, class Allocator, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct(const Callable& callable, Allocator);

	template <class Callable, class Allocator, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct(Callable&& callable, Allocator alloc);
	template <class Callable, class Allocator, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	construct(const Callable& callable, Allocator alloc);

	void operator()();
private:
	std::uint8_t myStorage[Callable_Max_Size_No_Heap_Alloc];
	job_callable_base* myCallable;
};
}
}

