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
class job_delegate;

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

class job_delegate_base
{
public:
	virtual ~job_delegate_base() = default;

	__forceinline virtual void operator()() = 0;

	virtual job_delegate_base* copy_construct_at(uint8_t* storage) = 0;
};
template <class Callable, class ...Args>
class job_delegate_impl : public job_delegate_base
{
public:
	job_delegate_impl(Callable call, Args&& ... args);
	job_delegate_impl(Callable call, const std::tuple<Args...>& args);

	__forceinline void operator()() override final;

	job_delegate_base* copy_construct_at(uint8_t* storage) override final;

private:
	const std::tuple<Args...> m_args;
	const Callable m_callable;
};
template<class Callable, class ...Args>
inline job_delegate_impl<Callable, Args...>::job_delegate_impl(Callable call, Args&& ...args)
	: m_callable(std::forward<Callable&&>(call))
	, m_args(std::forward<Args&&>(args)...)
{
}
template<class Callable, class ...Args>
inline job_delegate_impl<Callable, Args...>::job_delegate_impl(Callable call, const std::tuple<Args...>& args)
	: m_callable(std::forward<Callable&&>(call))
	, m_args(args)
{
}
template<class Callable, class ...Args>
inline void job_delegate_impl<Callable, Args...>::operator()()
{
	expand_tuple_in(m_callable, m_args);
}
template<class Callable, class ...Args>
inline job_delegate_base* job_delegate_impl<Callable, Args...>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::job_delegate_impl<Callable, Args...>(this->m_callable, m_args);
}
template <class Callable>
class job_delegate_impl<Callable> : public job_delegate_base
{
public:
	job_delegate_impl(Callable call);

	__forceinline void operator()() override final;

	job_delegate_base* copy_construct_at(uint8_t* storage) override final;

private:
	const Callable m_callable;
};
template<class Callable>
inline job_delegate_impl<Callable>::job_delegate_impl(Callable call)
	: m_callable(std::forward<Callable&&>(call))
{
}
template<class Callable>
inline void job_delegate_impl<Callable>::operator()()
{
	m_callable();
}
template<class Callable>
inline job_delegate_base* job_delegate_impl<Callable>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::job_delegate_impl<Callable>(m_callable);
}
}
class alignas(std::max_align_t) job_delegate
{
public:
	job_delegate() noexcept;
	job_delegate(const job_delegate & other);

	job_delegate& operator=(const job_delegate& other);
	job_delegate& operator=(job_delegate&& other);

	template <class Callable, class ...Args>
	job_delegate(Callable && call, jh_detail::allocator_type alloc, Args && ... args);

	void operator()();

	~job_delegate() noexcept;

private:
	bool has_allocated() const noexcept;

	template <std::size_t Size, std::enable_if_t<(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)> * = nullptr>
	uint8_t* fetch_storage(jh_detail::allocator_type alloc);
	template <std::size_t Size, std::enable_if_t<!(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)> * = nullptr>
	uint8_t* fetch_storage(jh_detail::allocator_type);

	uint8_t* put_allocated_storage(std::size_t size, jh_detail::allocator_type alloc);

	union
	{
		std::uint8_t m_storage[jh_detail::Callable_Max_Size_No_Heap_Alloc];
		struct
		{
			jh_detail::allocator_type m_allocator;
			std::uint8_t* m_callableBegin;
			std::size_t m_allocated;
		}m_allocFields;
	};
	jh_detail::job_delegate_base* m_callable;
};
template<class Callable, class ...Args>
	inline job_delegate::job_delegate(Callable&& call, jh_detail::allocator_type alloc, Args&& ... args)
		: m_storage {}
		, m_callable(new (fetch_storage<sizeof(jh_detail::job_delegate_impl<Callable, Args...>)>(alloc)) jh_detail::job_delegate_impl<Callable, Args...>(std::forward<Callable&&>(call), std::forward<Args&&>(args)...))
{
	static_assert(!(jh_detail::Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");
	static_assert(!(alignof(std::max_align_t) < alignof(Callable)), "Custom align callables unsupported");
}
template<std::size_t Size, std::enable_if_t<(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* job_delegate::fetch_storage(jh_detail::allocator_type alloc)
{
	return put_allocated_storage(Size, alloc);
}
template<std::size_t Size, std::enable_if_t<!(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* job_delegate::fetch_storage(jh_detail::allocator_type)
{
	return &m_storage[0];
}
}