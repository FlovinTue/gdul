// Copyright(c) 2020 Flovin Michaelsen
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

namespace gdul
{

namespace del_detail {

template <class Callable>
struct callable_full_signature3
{
	using type = void;
};

template <class Result, class Class, class ...Args>
struct callable_full_signature3<Result(Class::*)(Args...) const>
{
	using type = Result(Args...);
};


template <class Callable, class IsFunctionType>
struct callable_full_signature2
{
	using type = Callable;
};
template <class Callable>
struct callable_full_signature2<Callable, std::false_type>
{
	// Extract full signature from callable operator()
	using type = typename callable_full_signature3<decltype(&Callable::operator())>::type;
};


template <class Callable>
struct callable_full_signature
{
	using eval_type = std::conditional_t<std::is_function_v<Callable> || std::is_member_function_pointer_v<Callable>, std::true_type, std::false_type>;
	using type = typename callable_full_signature2<Callable, eval_type>::type;
};

template <std::size_t FirstArgument, class Result, class Tuple, class IndexSequence>
struct evaluate_partial_signature3 {};

template <std::size_t FirstArgument, class Result, class Tuple, std::size_t ...IndexSequence>
struct evaluate_partial_signature3<FirstArgument, Result, Tuple, std::index_sequence<IndexSequence...>>
{
	using signature = Result(std::tuple_element_t<IndexSequence + FirstArgument, Tuple>...);
};

template <class BoundArgsTuple, class Result>
struct evaluate_partial_signature2 {};

// Evaluate partial signature of functions
template <class BoundArgsTuple, class Result, class ...Args>
struct evaluate_partial_signature2<BoundArgsTuple, Result(Args...)>
{
	static constexpr std::size_t TupleSize = std::tuple_size_v<BoundArgsTuple>;

	using args_tuple = std::tuple<Args...>;
	using index_sequence = std::make_index_sequence<sizeof ...(Args) - TupleSize>;
	using signature = typename evaluate_partial_signature3<TupleSize, Result, args_tuple, index_sequence>::signature;
};

// Evaluate partial signature of class methods
template <class BoundArgsTuple, class Result, class Class, class ...Args>
struct evaluate_partial_signature2<BoundArgsTuple, Result(Class::*)(Args...)>
{
	static constexpr std::size_t TupleSize = std::tuple_size_v<BoundArgsTuple>;

	struct void_element {};

	// Can't use std::conditional_t because it'll try to evaluate type of std::tuple_element_t<0, BoundArgsTuple> even if it's empty
	using eval_first_arg_tuple_helper = std::invoke_result_t<decltype(std::tuple_cat<BoundArgsTuple, std::tuple<void_element>>), BoundArgsTuple, std::tuple<void_element>>;

	using first_bound_arg = std::tuple_element_t<0, eval_first_arg_tuple_helper>;

	static constexpr bool IsInstanceBound = std::is_same_v<std::decay_t<first_bound_arg>, Class*>;

	static_assert(TupleSize == 0 || IsInstanceBound, "First bound argument must be of class instance type");

	using args_tuple = std::conditional_t<IsInstanceBound, std::tuple<Args...>, std::tuple<Class*, Args...>>;

	static constexpr std::size_t BoundArguments = IsInstanceBound ? TupleSize - 1 : TupleSize;

	using index_sequence = std::make_index_sequence<std::tuple_size_v<args_tuple> -BoundArguments>;

	using signature = typename evaluate_partial_signature3<BoundArguments, Result, args_tuple, index_sequence>::signature;
};

template <class Callable, class ...BoundArgs>
struct evaluate_partial_signature
{
private:
	using raw_callable_type = std::remove_pointer_t<std::remove_reference_t<Callable>>;

public:
	using full_signature = typename callable_full_signature<raw_callable_type>::type;
	using signature = typename evaluate_partial_signature2<std::tuple<BoundArgs...>, full_signature>::signature;
};

using default_allocator = std::allocator<std::uint8_t>;

template <class Ret, class ...Args>
struct callable_wrapper_base
{
	inline virtual Ret operator()(Args&&... args) const = 0;

	virtual ~callable_wrapper_base() = default;
	virtual std::uint8_t* allocate() { assert(false); return nullptr; }
	virtual void destruct_allocated(callable_wrapper_base<Ret, Args...>*) { assert(false); }
	virtual callable_wrapper_base<Ret, Args...>* copy_construct_at(std::uint8_t*) = 0;
};
template <class Ret, class Callable,class ...Args>
struct callable_wrapper_impl_call : public callable_wrapper_base<Ret, Args...>
{
	callable_wrapper_impl_call(Callable callable)
		: m_callable(callable) {}

	inline Ret operator()(Args&&... args)  const override final{
		return std::invoke(m_callable, std::forward<Args>(args)...);
	}

	callable_wrapper_base<Ret, Args...>* copy_construct_at(std::uint8_t* block) override {
		return new (block) (callable_wrapper_impl_call<Ret, Callable, Args...>)(Callable(m_callable));
	}

	Callable m_callable;
};

template <class Ret, class Callable,class Allocator, class ...Args>
struct callable_wrapper_impl_call_alloc : public callable_wrapper_impl_call<Ret, Callable, Args...>
{
	callable_wrapper_impl_call_alloc(Callable callable, Allocator alloc)
		: callable_wrapper_impl_call<Ret, Callable, Args...>(callable)
		, m_allocator(alloc) {}

	std::uint8_t* allocate() override final{ 
		return m_allocator.allocate(sizeof(decltype(*this)));
	}
	void destruct_allocated(callable_wrapper_base<Ret, Args...>* obj) override final{ 
		Allocator alloc(m_allocator);
		constexpr std::size_t size(sizeof(decltype(*this)));
		obj->~callable_wrapper_base();
		alloc.deallocate((std::uint8_t*)obj, size);
	}
	callable_wrapper_base<Ret, Args...>* copy_construct_at(std::uint8_t* block) override final{
		return new (block) (callable_wrapper_impl_call_alloc<Ret, Callable, Allocator, Args...>)(this->m_callable, m_allocator);
	}

	Allocator m_allocator;
};

template <class Ret, class Callable,class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind : public callable_wrapper_base<Ret, Args...>
{
	callable_wrapper_impl_call_bind(Callable callable, BindTuple&& bind)
		: m_callable(callable)
		, m_boundTuple(std::forward<BindTuple>(bind)) {}

	inline Ret operator()(Args&&... args) const override final{
		return std::apply(m_callable, std::tuple_cat(m_boundTuple, std::forward_as_tuple(std::forward<Args>(args)...)));
	}

	callable_wrapper_base<Ret, Args...>* copy_construct_at(std::uint8_t* block) override {
		return new (block) (callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>)(m_callable, BindTuple(m_boundTuple));
	}

	Callable m_callable;
	BindTuple m_boundTuple;
};
template <class Ret, class Callable,class Allocator, class BindTuple, class ...Args>
struct callable_wrapper_impl_call_bind_alloc : public callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>
{
	callable_wrapper_impl_call_bind_alloc(Callable callable, BindTuple&& bind, Allocator alloc)
		: callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>(callable, std::forward<BindTuple>(bind))
		, m_allocator(alloc) {}

	std::uint8_t* allocate() override final{
		return m_allocator.allocate(sizeof(decltype(*this)));
	}
	void destruct_allocated(callable_wrapper_base<Ret, Args...>* obj) override final{
		Allocator alloc(m_allocator);
		constexpr std::size_t size(sizeof(decltype(*this)));
		obj->~callable_wrapper_base();
		alloc.deallocate((std::uint8_t*)obj, size);
	}
	callable_wrapper_base<Ret, Args...>* copy_construct_at(std::uint8_t* block) override final{
		return new (block) (callable_wrapper_impl_call_bind_alloc<Ret, Callable, Allocator, BindTuple, Args...>)(Callable(this->m_callable), BindTuple(this->m_boundTuple), m_allocator);
	}
	Allocator m_allocator;
};

constexpr std::size_t DelegateStorage = 56;

template <class Ret, class ...Args>
class alignas (alignof(std::max_align_t)) delegate_impl
{
public:
	using return_type = Ret;

	static constexpr std::size_t NumArgs = sizeof...(Args);

	inline delegate_impl() noexcept;
	inline ~delegate_impl() noexcept;

	inline Ret operator()(Args  ...args) const;

protected:
	template <class Callable, class Allocator, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>), std::enable_if_t<!(del_detail::DelegateStorage < Compare)>* = nullptr>
	void construct_call(Callable && callable, Allocator);

	template <class Callable, class Allocator, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>), std::enable_if_t<(del_detail::DelegateStorage < Compare)> * = nullptr>
	void construct_call(Callable && callable, Allocator alloc);

	template <class Callable, class Allocator, class BindTuple, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>), std::enable_if_t<!(del_detail::DelegateStorage < Compare)> * = nullptr>
	void construct_call_bind(Callable && callable, Allocator, BindTuple&& args);

	template <class Callable, class Allocator, class BindTuple, std::size_t Compare = sizeof(del_detail::callable_wrapper_impl_call_bind<Ret, Callable, BindTuple, Args...>), std::enable_if_t<(del_detail::DelegateStorage < Compare)> * = nullptr>
	void construct_call_bind(Callable && callable, Allocator alloc, BindTuple && args);

	void copy_from(const delegate_impl<Ret, Args...> & other);
	void move_from(delegate_impl<Ret, Args...> & other);
private:
	template <class Callable, class Allocator>
	void construct_call_alloc(Callable && callable, Allocator alloc);

	template <class Callable, class BindTuple, class Allocator>
	void construct_call_bind_alloc(Callable && callable, Allocator alloc, BindTuple && bound);

private:
	bool has_allocated() const;
	
	std::uint8_t m_storage[del_detail::DelegateStorage];

	del_detail::callable_wrapper_base<Ret, Args...>* m_callable;
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
	if ((std::uint8_t*)m_callable == &m_storage[0]){
		m_callable->~callable_wrapper_base();
		return;
	}
	
	if (m_callable){
		m_callable->destruct_allocated(m_callable);
	}
}
template<class Ret, class ...Args>
inline Ret delegate_impl<Ret, Args...>::operator()(Args ... args) const
{
	return m_callable->operator()(std::forward<Args>(args)...);
}
template<class Ret, class ...Args>
inline void delegate_impl<Ret, Args...>::copy_from(const delegate_impl<Ret, Args...>& other)
{
	if (other.has_allocated()) {
		std::uint8_t* const storage(other.m_callable->allocate());
		other.m_callable->copy_construct_at(storage);
		m_callable = (callable_wrapper_base<Ret, Args...>*)storage;
	}
	else {
		m_callable = other.m_callable->copy_construct_at(&m_storage[0]);
	}
}
template<class Ret, class ...Args>
inline void delegate_impl<Ret, Args...>::move_from(delegate_impl<Ret, Args...>& other)
{
	if (other.has_allocated()) {
		m_callable = other.m_callable;
		other.m_callable = nullptr;
	}
	else {
		copy_from(other);
	}
}
template<class Ret, class ...Args>
inline bool delegate_impl<Ret, Args...>::has_allocated() const
{
	return ((std::uint8_t*)m_callable < &m_storage[0]) || (&m_storage[del_detail::DelegateStorage - 1] < (std::uint8_t*)m_callable);
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, std::size_t Compare, std::enable_if_t<!(del_detail::DelegateStorage < Compare)>*>
void delegate_impl<Ret, Args...>::construct_call(Callable&& callable, Allocator)
{
	using type = del_detail::callable_wrapper_impl_call<Ret, Callable, Args...>;
	m_callable = new (&m_storage[0]) type((std::forward<Callable>(callable)));
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, std::size_t Compare, std::enable_if_t<(del_detail::DelegateStorage < Compare)>*>
void delegate_impl<Ret, Args...>::construct_call(Callable&& callable, Allocator alloc)
{
	construct_call_alloc(std::forward<Callable>(callable), alloc);
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, class BindTuple, std::size_t Compare, std::enable_if_t<(del_detail::DelegateStorage < Compare)>* >
inline void delegate_impl<Ret, Args...>::construct_call_bind(Callable&& callable, Allocator alloc, BindTuple&& bind)
{
	construct_call_bind_alloc(std::forward<Callable>(callable), alloc, std::forward<BindTuple>(bind));
}
template<class Ret, class ...Args>
template<class Callable, class Allocator, class BindTuple, std::size_t Compare, std::enable_if_t<!(del_detail::DelegateStorage < Compare)>* >
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
	using signature_type = Signature;

	delegate() noexcept = default;

	delegate(const delegate<Signature>& other) {
		this->copy_from(other);
	}
	delegate(delegate<Signature>&& other) {
		this->move_from(other);
	}
	delegate& operator=(const delegate<Signature>& other) {
		this->copy_from(other);
		return *this;
	}
	delegate& operator=(delegate<Signature>&& other) {
		this->move_from(other);
		return *this;
	}

	template <class Callable>
	delegate(Callable callable) {
		this->construct_call(std::move(callable), del_detail::default_allocator());
	}
	template <class Callable, class Allocator>
	delegate(std::pair<Callable, Allocator>&& callableAllocPair) {
		this->construct_call(std::move(callableAllocPair.first), callableAllocPair.second);
	}
	template <class Callable, class ...Args>
	delegate(Callable callable, Args&&... args) {
		this->construct_call_bind(std::move(callable), del_detail::default_allocator(), std::make_tuple(std::forward<Args>(args)...));
	}
	template <class Callable, class Allocator, class ...Args>
	delegate(std::pair<Callable, Allocator>&& callableAllocPair, Args&&... args) {
		this->construct_call_bind(std::move(callableAllocPair.first), std::move(callableAllocPair.second), std::make_tuple(std::forward<Args>(args)...));
	}
};

template <class Callable, class ...BoundArgs>
delegate<typename del_detail::evaluate_partial_signature<Callable, BoundArgs...>::signature> make_delegate(Callable&& callable, BoundArgs&& ... boundArgs)
{
	using signature = typename del_detail::evaluate_partial_signature<Callable, BoundArgs...>::signature;

	return delegate<signature>(std::forward<Callable>(callable), std::forward<BoundArgs>(boundArgs)...);
}

// Despite it's name, it will not allocate unless it has to. Simply a distinction for using an explicit allocator
template <class Callable, class Allocator, class ...BoundArgs>
delegate<typename del_detail::evaluate_partial_signature<Callable, BoundArgs...>::signature> allocate_delegate(Callable&& callable, Allocator alloc, BoundArgs&& ... boundArgs)
{
	using signature = typename del_detail::evaluate_partial_signature<Callable, BoundArgs...>::signature;

	return delegate<signature>(std::make_pair(std::forward<Callable>(callable), alloc), std::forward<BoundArgs>(boundArgs)...);
}

}