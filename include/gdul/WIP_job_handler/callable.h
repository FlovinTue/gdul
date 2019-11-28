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

class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;
};

template <class Callable>
class callable_impl : public callable_base
{
public:
	callable_impl(Callable&& callable_impl);

	void operator()() override;

private:
	Callable m_callable;
};
template<class Callable>
inline callable_impl<Callable>::callable_impl(Callable && callable_impl)
	: m_callable(std::forward<Callable&&>(callable_impl))
{
}
template<class Callable>
inline void callable_impl<Callable>::operator()()
{
	m_callable();
}
class alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) callable
{
public:
	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>* = nullptr>
	callable(Callable&& callable, allocator_type);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>* = nullptr>
	callable(Callable&& callable, allocator_type alloc);

	void operator()();

	~callable() noexcept;

private:
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

	callable_base* m_callable;
};
template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>*>
	inline callable::callable(Callable && callable, allocator_type alloc)
		: m_storage{}
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");

	m_allocFields.m_allocator = alloc;

	if (16 < alignof(Callable))
	{
		m_allocFields.m_allocated = sizeof(Callable) + alignof(Callable);
	}
	else
	{
		m_allocFields.m_allocated = sizeof(Callable);
	}
	m_allocFields.m_callableBegin = m_allocFields.m_allocator.allocate(m_allocFields.m_allocated);

	const std::uintptr_t callableBeginAsInt(reinterpret_cast<std::uintptr_t>(m_allocFields.m_callableBegin));
	const std::uintptr_t mod(callableBeginAsInt % alignof(Callable));
	const std::size_t offset(mod ? alignof(Callable) - mod : 0);

	m_callable = new (m_allocFields.m_callableBegin + offset) gdul::jh_detail::callable_impl(std::forward<Callable&&>(callable));
}
template<class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>*>
	inline callable::callable(Callable && callable, allocator_type)
		: m_storage{}
{
	m_callable = new (&m_storage[0]) gdul::jh_detail::callable_impl<Callable>(std::forward<Callable&&>(callable));
}
}
}