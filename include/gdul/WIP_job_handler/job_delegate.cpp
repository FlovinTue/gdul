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

#include "job_delegate.h"

namespace gdul
{
job_delegate::job_delegate() noexcept
	: job_delegate([]() {}, jh_detail::allocator_type())
{
}
job_delegate::job_delegate(const job_delegate& other)
	: m_storage{}
	, m_callable(other.m_callable->copy_construct_at(!other.has_allocated() ?
		&m_storage[0] :
		put_allocated_storage(other.m_allocFields.m_allocated, other.m_allocFields.m_allocator)))
{
}
job_delegate& job_delegate::operator=(const job_delegate& other)
{
	this->~job_delegate();
	new (this) job_delegate(other);

	return *this;
}
job_delegate& job_delegate::operator=(job_delegate&& other)
{
	if (other.has_allocated()) {
		std::swap(m_allocFields.m_allocated, other.m_allocFields.m_allocated);
		std::swap(m_allocFields.m_allocator, other.m_allocFields.m_allocator);
		std::swap(m_allocFields.m_callableBegin, other.m_allocFields.m_callableBegin);
		std::swap(m_callable, other.m_callable);

		return *this;
	}
	else{
		return operator=(other);
	}
}
bool job_delegate::operator()()
{
	return (*m_callable)();
}
job_delegate::~job_delegate() noexcept
{
	if (m_callable) {
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
bool job_delegate::has_allocated() const noexcept
{
	return (void*)m_callable != (void*)&m_storage[0];
}
uint8_t* job_delegate::put_allocated_storage(std::size_t size, jh_detail::allocator_type alloc)
{
	m_allocFields.m_allocator = alloc;
	m_allocFields.m_allocated = size;
	m_allocFields.m_callableBegin = m_allocFields.m_allocator.allocate(m_allocFields.m_allocated);

	return m_allocFields.m_callableBegin;
}
}
