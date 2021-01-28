#pragma once

#include <memory>
#include <cassert>
#include <vector>

namespace gdul {
namespace sv_detail {
template <class T, std::uint8_t LocalItems, class ParentAllocator>
class small_allocator;

template <class T, class SmallAllocator>
class small_allocator_proxy
{
public:
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const pointer;
	using size_type = std::size_t;
	using parent_allocator_type = typename SmallAllocator::parent_allocator_type::template rebind<T>::other;

	template <typename U>
	struct rebind
	{
		using other = small_allocator_proxy<U, SmallAllocator>;
	};

	small_allocator_proxy(SmallAllocator& smallAllocator);

	bool operator==(const small_allocator_proxy<T, SmallAllocator>& other) const;
	bool operator!=(const small_allocator_proxy<T, SmallAllocator>& other) const;
	bool operator==(const SmallAllocator& other) const;
	bool operator!=(const SmallAllocator& other) const;

	T* allocate(std::size_t count);
	void deallocate(T* p, std::size_t count) noexcept;

private:
	SmallAllocator& m_smallAllocator;
	parent_allocator_type m_parent;
};
template <class T, class SmallAllocator>
small_allocator_proxy<T, SmallAllocator>::small_allocator_proxy(SmallAllocator& smallAllocator)
	: m_smallAllocator(smallAllocator)
	, m_parent(smallAllocator.parent())
{

}
template <class T, class SmallAllocator>
inline bool small_allocator_proxy<T, SmallAllocator>::operator==(const small_allocator_proxy<T, SmallAllocator>& other) const
{
	return m_smallAllocator == other.m_smallAllocator;
}
template <class T, class SmallAllocator>
inline bool small_allocator_proxy<T, SmallAllocator>::operator!=(const small_allocator_proxy<T, SmallAllocator>& other) const
{
	return !operator==(other);
}
template <class T, class SmallAllocator>
inline bool small_allocator_proxy<T, SmallAllocator>::operator==(const SmallAllocator& other) const
{
	return m_smallAllocator == other;;
}
template <class T, class SmallAllocator>
inline bool small_allocator_proxy<T, SmallAllocator>::operator!=(const SmallAllocator& other) const
{
	return !operator==(other);
}
template <class T, class SmallAllocator>
inline void small_allocator_proxy<T, SmallAllocator>::deallocate(T* p, std::size_t count) noexcept
{
	if constexpr (std::is_same_v<value_type, typename SmallAllocator::value_type>) {
		m_smallAllocator.deallocate(p, count);
	}

}
template <class T, class SmallAllocator>
inline T* small_allocator_proxy<T, SmallAllocator>::allocate(std::size_t count)
{
	return m_parent.allocate(count);
}

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
		using other = small_allocator_proxy<U, small_allocator<T, LocalItems, ParentAllocator>>;
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
	small_allocator(const ParentAllocator& parent);

	T* allocate(size_type count);
	void deallocate(T* p, size_type count);

	ParentAllocator& parent();

	bool operator==(const small_allocator<T, LocalItems, ParentAllocator>& other) const = delete;
	bool operator!=(const small_allocator<T, LocalItems, ParentAllocator>& other) const = delete;

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
inline small_allocator<T, LocalItems, ParentAllocator>::small_allocator(const ParentAllocator& parent)
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
inline ParentAllocator& small_allocator<T, LocalItems, ParentAllocator>::parent()
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

	small_vector(small_vector<T, LocalItems, Allocator>&& other) noexcept;
	void swap(small_vector<T, LocalItems, Allocator>& other) noexcept;
};
template <class T, std::uint8_t LocalItems, class Allocator>
small_vector<T, LocalItems, Allocator>::small_vector(small_vector<T, LocalItems, Allocator>&& other) noexcept
{
	this->assign(other.begin(), other.end());
}
template <class T, std::uint8_t LocalItems, class Allocator>
void small_vector<T, LocalItems, Allocator>::swap(small_vector<T, LocalItems, Allocator>& other) noexcept
{
	small_vector local(other.begin(), other.end());

	other.assign(this->begin(), this->end());

	this->assign(local.begin(), local.end());
}
}

template <class T, std::uint8_t LocalItems = 6, class Allocator = std::allocator<T>>
using small_vector = sv_detail::small_vector<T, LocalItems, Allocator>;

}