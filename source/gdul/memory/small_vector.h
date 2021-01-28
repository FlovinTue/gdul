#pragma once

#include <memory>
#include <cassert>
#include <vector>

namespace gdul {
namespace sv_detail {
template <class T, std::uint8_t LocalItems, class ParentAllocator>
class small_allocator;

template <class T, class ParentAllocator = std::allocator<T>>
class small_allocator_proxy : public ParentAllocator
{
public:
	small_allocator_proxy() = default;

	template <class U, std::uint8_t LocalItems, class OtherParentAllocator>
	small_allocator_proxy(small_allocator<U, LocalItems, OtherParentAllocator>&& other)
		: ParentAllocator(other.get_parent())
	{
	}

	template <class U, std::uint8_t LocalItems, class OtherParentAllocator>
	small_allocator_proxy(const small_allocator<U, LocalItems, OtherParentAllocator>& other)
		: ParentAllocator(other.get_parent())
	{
	}
};

/// <summary>
/// Allocator with small local block. A bit special case, as it does not support copy or move construction
/// </summary>
/// <typeparam name="T">Value type</typeparam>
/// <typeparam name="LocalItems">Items stored locally</typeparam>
/// <typeparam name="ParentAllocator">Parent allocator used for larger blocks</typeparam>
template <class T, std::uint8_t LocalItems, class ParentAllocator>
class small_allocator
{
public:
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const pointer;
	using size_type = std::size_t;
	using parent_allocator_type = ParentAllocator;

	template <typename U>
	struct rebind
	{
		using other = small_allocator_proxy<U, typename parent_allocator_type::template rebind<U>::other>;
	};

	template <>
	struct rebind<T>
	{
		using other = small_allocator<T, LocalItems, ParentAllocator>;
	};

	small_allocator(const small_allocator&) = delete;
	small_allocator(small_allocator&&) = delete;
	small_allocator& operator=(const small_allocator&) = delete;
	small_allocator& operator=(small_allocator&&) = delete;

	small_allocator();
	small_allocator(ParentAllocator parent);

	T* allocate(size_type count);
	void deallocate(T* p, size_type count);

	ParentAllocator get_parent() const;

private:

	std::uint8_t locally_avaliable() const;

	bool locally_allocated(const T* p) const;

	void local_deallocate(size_type count);
	T* local_allocate(size_type count);

	struct alignas(alignof(T)) rep { std::uint8_t block[sizeof(T)]; };

	rep m_items[LocalItems];
	std::uint8_t m_at;
	std::uint8_t m_used;

	ParentAllocator m_allocator;
};
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline small_allocator<T, LocalItems, ParentAllocator>::small_allocator()
	: small_allocator<T, LocalItems, ParentAllocator>(ParentAllocator())
{
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline small_allocator<T, LocalItems, ParentAllocator>::small_allocator(ParentAllocator parent)
	: m_items{}
	, m_at(0)
	, m_used(0)
	, m_allocator(parent)
{
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline T* small_allocator<T, LocalItems, ParentAllocator>::allocate(size_type count)
{
	if (!(locally_avaliable() < count)) {
		return local_allocate(count);
	}

	return m_allocator.allocate(count);
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline void small_allocator<T, LocalItems, ParentAllocator>::deallocate(T* p, size_type count)
{
	if (locally_allocated(p)) {
		local_deallocate(count);
	}
	else {
		m_allocator.deallocate(p, count);
	}
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline ParentAllocator small_allocator<T, LocalItems, ParentAllocator>::get_parent() const
{
	return m_allocator;
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline std::uint8_t small_allocator<T, LocalItems, ParentAllocator>::locally_avaliable() const
{
	return LocalItems - m_at;
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline bool small_allocator<T, LocalItems, ParentAllocator>::locally_allocated(const T* p) const
{
	return !(p < reinterpret_cast<const T*>(&m_items[0])) && p < reinterpret_cast<const T*>(&m_items[LocalItems]);
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline void small_allocator<T, LocalItems, ParentAllocator>::local_deallocate(size_type count)
{
	assert(!(LocalItems < count) && "Local block cannot fit");

	m_used -= static_cast<std::uint8_t>(count);

	if (m_used == 0) {
		m_at = 0;
	}
}
template<class T, std::uint8_t LocalItems, class ParentAllocator>
inline T* small_allocator<T, LocalItems, ParentAllocator>::local_allocate(size_type count)
{
	assert(!(locally_avaliable() < count) && "Local block cannot fit");

	const std::uint8_t at(m_at);

	m_used += static_cast<std::uint8_t>(count);
	m_at += static_cast<std::uint8_t>(count);

	return reinterpret_cast<T*>(&m_items[at]);
}

template <class T, std::uint8_t LocalItems = 6, class Allocator = std::allocator<T>>
class small_vector : public std::vector<T, small_allocator<T, LocalItems, Allocator>>
{
public:
	using std::vector<T, small_allocator<T, LocalItems, Allocator>>::vector;
};
}

template <class T, std::uint8_t LocalItems = 6, class Allocator = std::allocator<T>>
using small_vector = sv_detail::small_vector<T, LocalItems, Allocator>;

}