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
#include <memory>
#include <cassert>
#include <tuple>

#define GDUL_ENABLE_IF_SIZE(type, size, op) template <class U = type, std::enable_if_t<sizeof(U) op size>* = nullptr>

namespace gdul
{

namespace del_detail {

template<class Callable, class Tuple, std::size_t ...IndexSeq>
constexpr auto apply_tuple_expansion(Callable&& call, Tuple&& tup, std::index_sequence<IndexSeq...>)
{
	return std::forward<Callable>(call)(std::get<IndexSeq>(std::forward<Tuple>(tup))...); tup;
}
template<class Callable, class Tuple>
constexpr auto call_with_tuple(Callable&& call, Tuple&& tup)
{
	using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
	return apply_tuple_expansion(std::forward<Callable>(call), std::forward<Tuple>(tup), Indices());
}

using default_allocator = std::allocator<std::uint8_t>;

template <class Ret, class ...Args>
struct callable_wrapper_base
{
	inline virtual Ret operator()(Args&&... args) = 0;

	virtual ~callable_wrapper_base() = default;
	virtual std::uint8_t* allocate(std::size_t) { assert(false); return nullptr; }
	virtual void destruct_allocated(callable_wrapper_base<Ret, Args...>*) { assert(false); }
	virtual void copy_construct_at(std::uint8_t*) = 0;
};
template <class Ret, class Callable,class ...Args>
struct callable_wrapper_impl_call : public callable_wrapper_base<Ret, Args...>
{
	callable_wrapper_impl_call(Callable&& callable)
		: m_callable(std::forward<Callable>(callable)) {}

	inline Ret operator()(Args&&... args) override {
		return m_callable(std::forward<Args>(args)...);
	}

	void copy_construct_at(std::uint8_t* block) override {
		new (block) (callable_wrapper_impl_call<Ret, Callable, Args...>)(Callable(m_callable));
	}

	const Callable m_callable;
};

template <class Ret, class Callable,class Allocator, class ...Args>
struct callable_wrapper_impl_call_alloc : public callable_wrapper_impl_call<Ret, Callable, Args...>
{
	callable_wrapper_impl_call_alloc(Callable&& callable, Allocator alloc)
		: callable_wrapper_impl_call<Ret, Callable, Args...>(std::forward<Callable>(callable))
		, m_allocator(alloc) {}

	std::uint8_t* allocate(std::size_t n) override { 
		return m_allocator.allocate(n);
	}
	void destruct_allocated(callable_wrapper_base<Ret, Args...>* obj) override { 
		Allocator alloc(m_allocator);
		constexpr std::size_t size(sizeof(decltype(*this)));
		obj->~callable_wrapper_base();
		alloc.deallocate((std::uint8_t*)obj, size);
	}

	Allocator m_allocator;
};

template <class Ret, class Callable,class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind : public callable_wrapper_base<Ret, Args...>
{
	callable_wrapper_impl_call_bind(Callable&& callable, BindTuple&& bind)
		: m_callable(std::forward<Callable>(callable))
		, m_boundTuple(std::forward<BindTuple>(bind)) {}

	inline Ret operator()(Args&&... args) override {
		return call_with_tuple(this->m_callable, std::tuple_cat(m_boundTuple, std::make_tuple(std::forward<Args>(args)...)));
	}

	void copy_construct_at(std::uint8_t* block) override {
		new (block) (callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>)(Callable(m_callable), BindTuple(m_boundTuple));
	}

	const Callable m_callable;
	const BindTuple m_boundTuple;
};
template <class Ret, class Callable,class Allocator, class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind_alloc : public callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>
{
	callable_wrapper_impl_call_bind_alloc(Callable&& callable, BindTuple&& bind, Allocator alloc)
		: callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>(std::forward<Callable>(callable), std::forward<BindTuple>(bind))
		, m_allocator(alloc) {}

	std::uint8_t* allocate(std::size_t n) override { 
		return m_allocator.allocate(n); 
	}
	void destruct_allocated(callable_wrapper_base<Ret, Args...>* obj) override {
		Allocator alloc(m_allocator);
		constexpr std::size_t size(sizeof(decltype(*this)));
		obj->~callable_wrapper_base();
		alloc.deallocate((std::uint8_t*)obj, size);
	}
	Allocator m_allocator;
};

constexpr std::size_t Delegate_Storage = 32;

template <class Ret, class ...Args>
class alignas (alignof(std::max_align_t)) delegate_impl
{
public:
	delegate_impl() noexcept;
	~delegate_impl() noexcept;

	Ret operator()(Args && ...args);

protected:
	template <class Callable, class Allocator, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>), std::enable_if_t<!(del_detail::Delegate_Storage < Compare)>* = nullptr>
	void construct_call(Callable && callable, Allocator);

	template <class Callable, class Allocator, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>), std::enable_if_t<(del_detail::Delegate_Storage < Compare)> * = nullptr>
	void construct_call(Callable && callable, Allocator alloc);

	template <class Callable, class Allocator, class BindTuple, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>), std::enable_if_t<!(del_detail::Delegate_Storage < Compare)> * = nullptr>
	void construct_call_bind(Callable && callable, Allocator, BindTuple&& args);

	template <class Callable, class Allocator, class BindTuple, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>), std::enable_if_t<(del_detail::Delegate_Storage < Compare)> * = nullptr>
	void construct_call_bind(Callable && callable, Allocator alloc, BindTuple && args);

private:
	template <class Callable, class Allocator>
	void construct_call_alloc(Callable && callable, Allocator alloc);

	template <class Callable, class BindTuple, class Allocator>
	void construct_call_bind_alloc(Callable && callable, Allocator alloc, BindTuple && bound);

private:
	bool has_allocated();
	
	del_detail::callable_wrapper_base<Ret, Args...>* m_callable;

	std::uint8_t m_storage[del_detail::Delegate_Storage];
};

template <class Signature>
struct delegate_sig_def;
template <class Ret, class ...Args>
struct delegate_sig_def<Ret(Args...)> { using impl = delegate_impl<Ret, Args...>; };
template<class Ret, class ...Args>
delegate_impl<Ret, Args...>::delegate_impl() noexcept
	: m_callable(nullptr)
	, m_storage{}
{}
template<class Ret, class ...Args>
inline delegate_impl<Ret, Args...>::~delegate_impl() noexcept
{
	if (!has_allocated()) {
		m_callable->~callable_wrapper_base();
	}
	else {
		m_callable->destruct_allocated(m_callable);
	}
}
template<class Ret, class ...Args>
inline Ret delegate_impl<Ret, Args...>::operator()(Args&& ... args)
{
	return m_callable->operator()(std::forward<Args>(args)...);
}
template<class Ret, class ...Args>
inline bool delegate_impl<Ret, Args...>::has_allocated()
{
	return ((std::uint8_t*)m_callable < &m_storage[0]) | (&m_storage[del_detail::Delegate_Storage - 1] < (std::uint8_t*)m_callable);
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, std::size_t Compare, std::enable_if_t<!(del_detail::Delegate_Storage < Compare)>*>
void delegate_impl<Ret, Args...>::construct_call(Callable&& callable, Allocator)
{
	using type = del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>;
	m_callable = new (&m_storage[0]) type((std::forward<Callable>(callable)));
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, std::size_t Compare, std::enable_if_t<(del_detail::Delegate_Storage < Compare)>*>
void delegate_impl<Ret, Args...>::construct_call(Callable&& callable, Allocator alloc)
{
	construct_call_alloc(std::forward<Callable>(callable), alloc);
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, class BindTuple, std::size_t Compare, std::enable_if_t<(del_detail::Delegate_Storage < Compare)>* >
inline void delegate_impl<Ret, Args...>::construct_call_bind(Callable&& callable, Allocator alloc, BindTuple&& bind)
{
	construct_call_bind_alloc(std::forward<Callable>(callable), alloc, std::forward<BindTuple>(bind));
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, class BindTuple, std::size_t Compare, std::enable_if_t<!(del_detail::Delegate_Storage < Compare)>* >
inline void delegate_impl<Ret, Args...>::construct_call_bind(Callable&& callable, Allocator, BindTuple&& bound)
{
	using type = del_detail::callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>;
	m_callable = new (&m_storage[0]) type(std::forward<Callable>(callable), std::forward<BindTuple>(bound));
}
template<class Ret, class ...Args>
template<class Callable, class Allocator>
inline void delegate_impl<Ret, Args...>::construct_call_alloc(Callable&& callable, Allocator alloc)
{
	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;
	using type = del_detail::callable_wrapper_impl_call_alloc<Ret, Callable, allocator_type, Args...>;
	allocator_type rebound(alloc);
	m_callable = new (rebound.allocate(sizeof(type))) type(std::forward<Callable>(callable), rebound);
}
template<class Ret, class ...Args>
template<class Callable, class BindTuple, class Allocator>
inline void delegate_impl<Ret, Args...>::construct_call_bind_alloc(Callable&& callable, Allocator alloc, BindTuple&& bind)
{
	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;
	using type = del_detail::callable_wrapper_impl_call_bind_alloc<Ret, Callable, allocator_type, BindTuple, Args...>;
	allocator_type rebound(alloc);
	m_callable = new (rebound.allocate(sizeof(type))) type(std::forward<Callable>(callable), std::forward<BindTuple>(bind), rebound);
}
}
template <class Signature>
class delegate : public del_detail::delegate_sig_def<Signature>::impl
{
public:

	template <class Callable>
	delegate(Callable callable) {
		this->construct_call(std::move(callable), del_detail::default_allocator());
	}
	template <class Callable, class Allocator>
	delegate(Callable callable, Allocator alloc) {
		this->construct_call(std::move(callable), alloc);
	}
	template <class Callable, class ...Args>
	delegate(Callable callable, std::tuple<Args...>&& args) {
		this->construct_call_bind(std::move(callable), del_detail::default_allocator(), std::forward<std::tuple<Args...>>(args));
	}
	template <class Callable, class Allocator, class ...Args>
	delegate(Callable callable, Allocator alloc, std::tuple<Args...>&& args) {
		this->construct_call_bind(std::move(callable), alloc, std::forward<std::tuple<Args>>(args));
	}
};

}