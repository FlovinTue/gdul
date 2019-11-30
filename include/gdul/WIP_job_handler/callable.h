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

#include <gdul/WIP_job_handler/job_handler_commons.h>

namespace gdul
{
namespace jh_detail
{

template<class Callable, class Tuple, std::size_t ...IndexSeq>
constexpr void _expand_tuple_in_apply(Callable&& call, Tuple&& tup, std::index_sequence<IndexSeq...>)
{
	std::forward<Callable&&>(call)(std::get<IndexSeq>(std::forward<Tuple&&>(tup))...); tup;
}
template<class Callable, class Tuple>
constexpr void expand_tuple_in(Callable&& call, Tuple&& tup)
{
	using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
	_expand_tuple_in_apply(std::forward<Callable&&>(call), std::forward<Tuple&&>(tup), Indices());
}

class callable_base
{
public:
	virtual ~callable_base() = default;

	__forceinline virtual void operator()() = 0;

	virtual callable_base* copy_construct_at(uint8_t* storage) = 0;
};
template <class Callable, class ...Args>
class callable_impl : public callable_base
{
public:
	callable_impl(Callable call, Args&& ... args);
	callable_impl(Callable call, const std::tuple<Args...>& args);

	__forceinline void operator()() override final;

	callable_base* copy_construct_at(uint8_t* storage) override final;

private:
	const std::tuple<Args...> m_args;
	const Callable m_callable;
};
template<class Callable, class ...Args>
inline callable_impl<Callable, Args...>::callable_impl(Callable call, Args&& ...args)
	: m_callable(std::forward<Callable&&>(call))
	, m_args(std::forward<Args&&>(args)...)
{
}
template<class Callable, class ...Args>
inline callable_impl<Callable, Args...>::callable_impl(Callable call, const std::tuple<Args...>& args)
	: m_callable(std::forward<Callable&&>(call))
	, m_args(args)
{
}
template<class Callable, class ...Args>
inline void callable_impl<Callable, Args...>::operator()()
{
	expand_tuple_in(m_callable, m_args);
}
template<class Callable, class ...Args>
inline callable_base* callable_impl<Callable, Args...>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::callable_impl<Callable, Args...>(this->m_callable, m_args);
}
template <class Callable>
class callable_impl<Callable> : public callable_base
{
public:
	callable_impl(Callable call);

	__forceinline void operator()() override final;

	callable_base* copy_construct_at(uint8_t* storage) override final;

private:
	const Callable m_callable;
};
template<class Callable>
inline callable_impl<Callable>::callable_impl(Callable call)
	: m_callable(std::forward<Callable&&>(call))
{
}
template<class Callable>
inline void callable_impl<Callable>::operator()()
{
	m_callable();
}
template<class Callable>
inline callable_base* callable_impl<Callable>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::callable_impl<Callable>(m_callable);
}

class alignas(std::max_align_t) callable
{
public:
	callable() noexcept;
	callable(const callable & other);

	callable& operator=(const callable&) = delete;
	callable& operator=(callable&&) = delete;

	template <class Callable, class ...Args>
	callable(Callable && call, allocator_type alloc, Args && ... args);

	void operator()();

	~callable() noexcept;

private:
	bool has_allocated() const noexcept;

	template <std::size_t Size, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < Size)> * = nullptr>
	uint8_t* fetch_storage(allocator_type alloc);
	template <std::size_t Size, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < Size)> * = nullptr>
	uint8_t* fetch_storage(allocator_type);

	uint8_t* put_allocated_storage(std::size_t size, allocator_type alloc);

	union
	{
		std::uint8_t m_storage[Callable_Max_Size_No_Heap_Alloc];
		struct
		{
			allocator_type m_allocator;
			std::uint8_t* m_callableBegin;
			std::size_t m_allocated;
		}m_allocFields;
	};
	callable_base* const m_callable;
};
template<class Callable, class ...Args>
	inline callable::callable(Callable&& call, allocator_type alloc, Args&& ... args)
		: m_storage {}
		, m_callable(new (fetch_storage<sizeof(callable_impl<Callable, Args...>)>(alloc)) callable_impl<Callable, Args...>(std::forward<Callable&&>(call), std::forward<Args&&>(args)...))
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");
	static_assert(!(alignof(std::max_align_t) < alignof(Callable)), "Custom align callables unsupported");
}
template<std::size_t Size, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* callable::fetch_storage(allocator_type alloc)
{
	return put_allocated_storage(Size, alloc);
}
template<std::size_t Size, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* callable::fetch_storage(allocator_type)
{
	const std::size_t size(Size);
	return &m_storage[0];
}
}
}