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
template <class RetVal, class InputArg>
class job_delegate;

namespace jh_detail
{
template<class Callable, class Tuple, std::size_t ...IndexSeq>
constexpr auto _expand_tuple_in_apply(Callable&& call, Tuple&& tup, std::index_sequence<IndexSeq...>)
{
	return std::forward<Callable&&>(call)(std::get<IndexSeq>(std::forward<Tuple&&>(tup))...); tup;
}
template<class Callable, class Tuple>
constexpr auto expand_tuple_in(Callable&& call, Tuple&& tup)
{
	using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
	return _expand_tuple_in_apply(std::forward<Callable&&>(call), std::forward<Tuple&&>(tup), Indices());
}

template <class ReturnType, class InputArg>
class job_delegate_base
{
public:
	virtual ~job_delegate_base() = default;

	__forceinline virtual ReturnType operator()(InputArg) = 0;

	virtual job_delegate_base* copy_construct_at(uint8_t* storage) = 0;
};
template <class Callable, class RetVal, class InputArg, class ...StaticArgs>
class job_delegate_impl : public job_delegate_base<RetVal, InputArg>
{
public:
	job_delegate_impl(Callable call, StaticArgs&& ... args);
	job_delegate_impl(Callable call, const std::tuple<StaticArgs...>& args);

	__forceinline bool operator()() override final;

	job_delegate_base<RetVal, InputArg>* copy_construct_at(uint8_t* storage) override final;

private:

	const std::tuple<StaticArgs...> m_staticArgs;
	const Callable m_callable;
};
template<class Callable, class RetVal, class InputArg, class ...StaticArgs>
inline job_delegate_impl<Callable, RetVal, InputArg, StaticArgs...>::job_delegate_impl(Callable call, StaticArgs&& ...args)
	: m_callable(std::forward<Callable&&>(call))
	, m_staticArgs(std::forward<StaticArgs&&>(args)...)
{
}
template<class Callable, class RetVal, class InputArg, class ...StaticArgs>
inline job_delegate_impl<Callable, RetVal, InputArg, StaticArgs...>::job_delegate_impl(Callable call, const std::tuple<StaticArgs...>& args)
	: m_callable(std::forward<Callable&&>(call))
	, m_staticArgs(args)
{
}
template<class Callable, class RetVal, class InputArg, class ...StaticArgs>
inline bool job_delegate_impl<Callable, RetVal, InputArg, StaticArgs...>::operator()()
{
	return m_callable();
}
template<class Callable, class RetVal, class InputArg, class ...StaticArgs>
inline job_delegate_base<RetVal, InputArg>* job_delegate_impl<Callable, RetVal, InputArg, StaticArgs...>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::job_delegate_impl<Callable, RetVal, InputArg, StaticArgs...>(this->m_callable, m_staticArgs);
}
template <class Callable, class RetVal, class InputArg>
class job_delegate_impl<Callable, RetVal, InputArg> : public job_delegate_base<RetVal, InputArg>
{
public:
	job_delegate_impl(Callable call);

	__forceinline bool operator()() override final;

	job_delegate_base<RetVal, InputArg>* copy_construct_at(uint8_t* storage) override final;

private:
	const Callable m_callable;
};
template<class Callable, class RetVal, class InputArg>
inline job_delegate_impl<Callable, RetVal, InputArg>::job_delegate_impl(Callable call)
	: m_callable(std::forward<Callable&&>(call))
{
}
template<class Callable, class RetVal, class InputArg>
inline bool job_delegate_impl<Callable, RetVal, InputArg>::operator()()
{
	return m_callable();
}
template<class Callable, class RetVal, class InputArg>
inline job_delegate_base<RetVal, InputArg>* job_delegate_impl<Callable, RetVal, InputArg>::copy_construct_at(uint8_t* storage)
{
	return new (storage) gdul::jh_detail::job_delegate_impl<Callable, RetVal, InputArg>(m_callable);
}
}
template <class RetVal, class InputArg>
class alignas(std::max_align_t) job_delegate
{
public:
	job_delegate() noexcept;
	job_delegate(const job_delegate & other);

	job_delegate& operator=(const job_delegate& other);
	job_delegate& operator=(job_delegate&& other);

	template <class Callable, class ...StaticArgs>
	job_delegate(Callable && call, jh_detail::allocator_type alloc, StaticArgs && ... StaticArgs);

	bool operator()();

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
	jh_detail::job_delegate_base<void, void>* m_callable;
};
template <class RetVal, class InputArg>
template<class Callable, class ...StaticArgs>
	inline job_delegate<RetVal, InputArg>::job_delegate(Callable&& call, jh_detail::allocator_type alloc, StaticArgs&& ... args)
		: m_storage {}
		, m_callable(new (fetch_storage<sizeof(jh_detail::job_delegate_impl<void, void, Callable, args...>)>(alloc)) jh_detail::job_delegate_impl<void, Callable, args...>(std::forward<Callable&&>(call), std::forward<StaticArgs&&>(args)...))
{
	static_assert(!(jh_detail::Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");
	static_assert(!(alignof(std::max_align_t) < alignof(Callable)), "Custom align callables unsupported");
}
template <class RetVal, class InputArg>
template<std::size_t Size, std::enable_if_t<(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* job_delegate<RetVal, InputArg>::fetch_storage(jh_detail::allocator_type alloc)
{
	return put_allocated_storage(Size, alloc);
}
template <class RetVal, class InputArg>
template<std::size_t Size, std::enable_if_t<!(jh_detail::Callable_Max_Size_No_Heap_Alloc < Size)>*>
inline uint8_t* job_delegate<RetVal, InputArg>::fetch_storage(jh_detail::allocator_type)
{
	return &m_storage[0];
}
template <class RetVal, class InputArg>
job_delegate<RetVal, InputArg>::job_delegate() noexcept
	: job_delegate([]() {}, jh_detail::allocator_type())
{
}
template <class RetVal, class InputArg>
job_delegate<RetVal, InputArg>::job_delegate(const job_delegate& other)
	: m_storage{}
	, m_callable(other.m_callable->copy_construct_at(!other.has_allocated() ?
		&m_storage[0] :
		put_allocated_storage(other.m_allocFields.m_allocated, other.m_allocFields.m_allocator)))
{
}
template <class RetVal, class InputArg>
job_delegate<RetVal, InputArg>& job_delegate<RetVal, InputArg>::operator=(const job_delegate& other)
{
	this->~job_delegate();
	new (this) job_delegate(other);

	return *this;
}
template <class RetVal, class InputArg>
job_delegate<RetVal, InputArg>& job_delegate<RetVal, InputArg>::operator=(job_delegate&& other)
{
	if (other.has_allocated())
	{
		std::swap(m_allocFields.m_allocated, other.m_allocFields.m_allocated);
		std::swap(m_allocFields.m_allocator, other.m_allocFields.m_allocator);
		std::swap(m_allocFields.m_callableBegin, other.m_allocFields.m_callableBegin);
		std::swap(m_callable, other.m_callable);

		return *this;
	}
	else
	{
		return operator=(other);
	}
}
template <class RetVal, class InputArg>
bool job_delegate<RetVal, InputArg>::operator()()
{
	return (*m_callable)();
}
template <class RetVal, class InputArg>
job_delegate<RetVal, InputArg>::~job_delegate() noexcept
{
	if (m_callable)
	{
		m_callable->~job_delegate_base();

		if (has_allocated())
		{
			m_allocFields.m_allocator.deallocate(m_allocFields.m_callableBegin, m_allocFields.m_allocated);
			using allocator_type = jh_detail::allocator_type;
			m_allocFields.m_allocator.~allocator_type();
		}
	}

	std::uninitialized_fill_n(m_storage, jh_detail::Callable_Max_Size_No_Heap_Alloc, std::uint8_t(0));
}
template <class RetVal, class InputArg>
bool job_delegate<RetVal, InputArg>::has_allocated() const noexcept
{
	return (void*)m_callable != (void*)&m_storage[0];
}
template <class RetVal, class InputArg>
uint8_t* job_delegate<RetVal, InputArg>::put_allocated_storage(std::size_t size, jh_detail::allocator_type alloc)
{
	m_allocFields.m_allocator = alloc;
	m_allocFields.m_allocated = size;
	m_allocFields.m_callableBegin = m_allocFields.m_allocator.allocate(m_allocFields.m_allocated);

	return m_allocFields.m_callableBegin;
}
}