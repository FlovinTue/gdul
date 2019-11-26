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
#pragma warning(push)
#pragma warning(disable : 4324)

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/callable.h>
#include <gdul/WIP_job_handler/chunk_allocator.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul{

class job_handler;
namespace job_handler_detail {

class callable_base;
class alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) job_impl
{
public:
	using allocator_type = gdul::job_handler_detail::allocator_type; 

	using job_impl_shared_ptr = shared_ptr<job_impl>;
	using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl>;
	using job_impl_raw_ptr = raw_ptr<job_impl>;
	using chunk_pool_type = chunk_pool<alloc_size_make_shared<job_impl, chunk_allocator<job_impl, chunk_pool<>>()
		
	job_impl() = default;

	template <class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(job_handler* handler, Callable&& callable, std::uint8_t priority, allocator_type);
	template <class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>* = nullptr>
	job_impl(job_handler* handler, Callable&& callable, std::uint8_t priority, allocator_type alloc);
	~job_impl();
	
	void operator()();

	bool try_attach_child(job_impl_shared_ptr child);

	std::uint8_t get_priority() const;

	void add_dependencies(std::uint32_t n = 1);
	std::uint32_t remove_dependencies(std::uint32_t n = 1);

	bool enable();
	bool has_sibling() const;

	job_handler* get_handler() const;

	bool is_finished() const;
private:
	void set_sibling(job_impl_shared_ptr sibling);

	void detach_children();

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
	job_handler* const m_handler;

	std::atomic<std::uint32_t> m_dependencies;

	std::atomic<bool> m_finished;

	const std::uint8_t m_priority;
};

template<class Callable, std::enable_if_t<(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(job_handler* handler, Callable && callable, std::uint8_t priority, allocator_type alloc)
	: m_storage{}
	, m_callable(nullptr)
	, m_finished(false)
	, m_firstChild(nullptr)
	, m_firstSibling(nullptr)
	, m_priority(priority)
	, m_handler(handler)
	, m_dependencies(Job_Max_Dependencies)
{
	static_assert(!(Callable_Max_Size_No_Heap_Alloc < sizeof(m_allocFields)), "too high size / alignment on allocator_type");

	m_allocFields.m_allocator = alloc;

	if (16 < alignof(Callable)) {
		m_allocFields.m_allocated = sizeof(Callable) + alignof(Callable);
	}
	else {
		m_allocFields.m_allocated = sizeof(Callable);
	}
	m_allocFields.m_callableBegin = m_allocFields.m_allocator.allocate(m_allocFields.m_allocated);

	const std::uintptr_t callableBeginAsInt(reinterpret_cast<std::uintptr_t>(m_allocFields.m_callableBegin));
	const std::uintptr_t mod(callableBeginAsInt % alignof(Callable));
	const std::size_t offset(mod ? alignof(Callable) - mod : 0);

	m_callable = new (m_allocFields.m_callableBegin + offset) gdul::job_handler_detail::callable(std::forward<Callable&&>(callable));
}

template<class Callable, std::enable_if_t<!(Callable_Max_Size_No_Heap_Alloc < sizeof(Callable))>*>
inline job_impl::job_impl(job_handler* handler, Callable && callable, std::uint8_t priority, allocator_type)
	: m_storage{}
	, m_callable(nullptr)
	, m_finished(false)
	, m_firstChild(nullptr)
	, m_firstSibling(nullptr)
	, m_handler(handler)
	, m_priority(priority)
	, m_dependencies(Job_Max_Dependencies)
{
	m_callable = new (&m_storage[0]) gdul::job_handler_detail::callable<Callable>(std::forward<Callable&&>(callable));
}
}
}

#pragma warning(pop)

