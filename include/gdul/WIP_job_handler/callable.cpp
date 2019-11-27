#include "callable.h"

namespace gdul
{
namespace jh_detail
{
void callable::operator()()
{
	(*m_callable)();
}
callable::~callable() noexcept
{
	m_callable->~callable_base();

	if ((void*)m_callable != (void*)&m_storage[0])
	{
		m_allocFields.m_allocator.deallocate(m_allocFields.m_callableBegin, m_allocFields.m_allocated);
		m_allocFields.m_allocator.~allocator_type();
	}
	m_callable = nullptr;

	std::uninitialized_fill_n(m_storage, Callable_Max_Size_No_Heap_Alloc, std::uint8_t(0));
}
}
}
