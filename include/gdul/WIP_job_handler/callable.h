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

	virtual callable_base* copy_construct_at(uint8_t* storage) = 0;
};

template <class Callable>
class callable_impl : public callable_base
{
public:
	callable_impl(Callable callable_impl);

	void operator()() override final;

	callable_base* copy_construct_at(uint8_t* storage) override final;

private:
	Callable m_callable;
};
template<class Callable>
inline callable_impl<Callable>::callable_impl(Callable callable_impl)
	: m_callable(std::forward<Callable&&>(callable_impl))
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
class alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) callable
{
public:
	callable(const callable & other);

	callable& operator=(const callable&) = delete;
	callable& operator=(callable&&) = delete;

	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>* = nullptr>
	callable(Callable&& call, allocator_type);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>* = nullptr>
	callable(Callable&& call, allocator_type alloc);

	void operator()();

	~callable() noexcept;

private:
	bool has_allocated() const noexcept;
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

	callable_base* m_callable;
};
template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>*>
	inline callable::callable(Callable && call, allocator_type alloc)
		: m_storage{}
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");
	static_assert(!(alignof(std::max_align_t) < alignof(Callable)), "Custom align callables unsupported");

	std::uint8_t* const storage(put_allocated_storage(sizeof(Callable), alloc));

	m_callable = new (storage) gdul::jh_detail::callable_impl(std::forward<Callable&&>(call));
}
template<class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(callable_impl<Callable>))>*>
	inline callable::callable(Callable && call, allocator_type)
		: m_storage{}
{
	m_callable = new (&m_storage[0]) gdul::jh_detail::callable_impl<Callable>(std::forward<Callable&&>(call));
}
}
}