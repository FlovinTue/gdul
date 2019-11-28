#include "callable.h"

namespace gdul
{
namespace jh_detail
{
callable::callable(const callable& other)
	: m_storage{}
{
	uint8_t* storage(nullptr);

	if (!other.has_allocated())
	{
		storage = &m_storage[0];
	}
	else
	{
		storage = put_allocated_storage(other.m_allocFields.m_allocated, other.m_allocFields.m_allocator);
	}
	m_callable = other.m_callable->copy_construct_at(storage);
}
void callable::operator()()
{
	(*m_callable)();
}
callable::~callable() noexcept
{
	m_callable->~callable_base();

	if (has_allocated())
	{
		m_allocFields.m_allocator.deallocate(m_allocFields.m_callableBegin, m_allocFields.m_allocated);
		m_allocFields.m_allocator.~allocator_type();
	}
	m_callable = nullptr;

	std::uninitialized_fill_n(m_storage, Callable_Max_Size_No_Heap_Alloc, std::uint8_t(0));
}
bool callable::has_allocated() const noexcept
{
	return (void*)m_callable != (void*)&m_storage[0];
}
uint8_t* callable::put_allocated_storage(std::size_t size, allocator_type alloc)
{
	m_allocFields.m_allocator = alloc;
	m_allocFields.m_allocated = size;
	m_allocFields.m_callableBegin = m_allocFields.m_allocator.allocate(m_allocFields.m_allocated);

	return m_allocFields.m_callableBegin;
}
}
}
