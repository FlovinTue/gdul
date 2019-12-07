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

#include <stdint.h>
#include <type_traits>

namespace gdul
{

namespace del_detail {

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

using default_allocator = std::allocator<std::uint8_t>;

template <class Ret, class ...Args>
struct callable_wrapper_base
{
	inline virtual Ret operator()(Args&&... args) = 0;

	virtual ~callable_wrapper_base() = 0;
	virtual std::uint8_t* allocate(std::size_t) { return nullptr; }
	virtual std::uint8_t* allocate(std::uint8_t*, std::size_t) {}
	virtual void copy_construct_at(std::uint8_t*) = 0;
};
template <class Callable, class ...Args>
struct callable_wrapper_impl_call : public callable_wrapper_base<std::result_of_t<Callable>, Args...>
{
	callable_wrapper_impl_call(Callable&& callable)
		: m_callable(std::forward<Callable&&>(callable)) {}

	inline std::result_of_t<Callable> operator()(Args&&... args) override {
		return m_callable(std::forward<Args&&>(args)...);
	}

	const Callable m_callable;
};

template <class Callable, class Allocator, class ...Args>
struct callable_wrapper_impl_call_alloc : public callable_wrapper_impl_call<Callable, Args...>
{
	callable_wrapper_impl_call_alloc(Callable&& callable, Allocator alloc)
		: callable_wrapper_impl_call<Callable, Args...>(std::forward<Callable&&>(callable))
		, m_allocator(alloc) {}

	std::uint8_t* allocate(std::size_t n) override { return m_allocator.allocate(n); }
	void deallocate(std::uint8_t* block, std::size_t n) override { m_allocator.deallocate(block, n); }

	const Allocator m_allocator;
};

template <class Callable, class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind : public callable_wrapper_impl_call<Callable, Args...>
{
	callable_wrapper_impl_call_bind(Callable&& callable, BindTuple&& bind)
		: callable_wrapper_impl_call<Callable, Args...>(std::forward<Callable&&>(callable))
		, m_boundArgs(std::forward<BindTuple&&>(bind)) {}

	inline std::result_of_t<Callable> operator()(Args&&... args) override {
		return expand_tuple_in(this->m_callable, m_boundArgs);
	}

	const BindTuple m_boundArgs;
};
template <class Callable, class Allocator, class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind_alloc : public callable_wrapper_impl_call_bind<Callable, Args...>
{
	callable_wrapper_impl_call_bind_alloc(Callable&& callable, BindTuple&& bind, Allocator alloc)
		: callable_wrapper_impl_call_bind<Callable, Args...>(std::forward<Callable&&>(callable), std::forward<BindTuple&&>(bind))
		, m_allocator(alloc) {}

	std::uint8_t* allocate(std::size_t n) override { return m_allocator.allocate(n); }
	void deallocate(std::uint8_t* block, std::size_t n) override { m_allocator.deallocate(block, n); }

	const Allocator m_allocator;
};

constexpr std::size_t Delegate_Storage = 32;

template <class Ret, class ...Args>
class alignas (alignof(std::max_align_t)) delegate_impl
{
public:
	delegate_impl() noexcept;

	template <class Callable>
	delegate_impl(Callable && callable);


private:
	template <std::size_t Size, class Allocator>
	void put_storage_at_callable(Allocator alloc);

	union
	{
		std::uint8_t m_storage[del_detail::Delegate_Storage];
		std::size_t m_allocated;
	};
	del_detail::callable_wrapper_base<Ret, Args...>* m_callable;
};

template <class Sig>
struct delegate_sig_def;
template <class Ret, class ...Args>
struct delegate_sig_def<Ret(Args...)> { using impl = delegate_impl<Ret, Args...>; };
template<class Ret, class ...Args>
delegate_impl<Ret, Args...>::delegate_impl() noexcept
	: m_callable(nullptr)
	, m_storage{}
{
}
template<class Ret, class ...Args>
template<class Callable>
delegate_impl<Ret, Args...>::delegate_impl(Callable&& callable)
	: delegate_impl()
{
	using type = del_detail::callable_wrapper_impl_call<Callable, Args...>;
	put_storage_at_callable<sizeof(type), del_detail::default_allocator>(del_detail::default_allocator());
	new (&m_callable) type((std::forward<Callable&&>(callable)));
}
template<class Ret, class ...Args>
template<std::size_t Size, class Allocator>
void delegate_impl<Ret, Args...>::put_storage_at_callable(Allocator alloc)
{
	if (del_detail::Delegate_Storage < Size) {
		m_callable = (del_detail::callable_wrapper_base<Ret, Args...>*) & m_storage[0];
	}
	else {
		m_callable = (del_detail::callable_wrapper_base<Ret, Args...>*) alloc.allocate(Size);
	}
}
}
template <class Sig>
class delegate : public del_detail::delegate_sig_def<Sig>::impl {};

}