// Copyright(c) 2021 Flovin Michaelsen
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

#include <gdul/utility/type_traits.h>

#include <atomic>
#include <memory>
#include <cassert>

namespace gdul {

namespace sa_detail {
class scratch_pad_base;
}

template <std::size_t StorageSize, std::size_t StorageAlignment>
class scratch_pad;


/// <summary>
/// Allocator associated with a scratch_pad. Stl compatible.
/// </summary>
/// <typeparam name="T">Value type</typeparam>
/// <typeparam name="ParentAllocator">Allocator used when scratch pad runs out of space</typeparam>
template <class T, class ParentAllocator = std::allocator<T>>
class scratch_allocator
{
public:
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const pointer;
	using size_type = std::size_t;
	using parent_allocator_type = typename std::allocator_traits<ParentAllocator>::template rebind_alloc<T>;

	using propagate_on_container_copy_assignment = std::false_type;
	using propagate_on_container_move_assignment = std::false_type;
	using propagate_on_container_swap = std::false_type;

	using is_always_equal = std::false_type;

	template <typename U>
	struct rebind
	{
		using other = scratch_allocator<U, typename std::allocator_traits<parent_allocator_type>::template rebind_alloc<U>>;
	};

	template <class U, class V>
	scratch_allocator(const scratch_allocator<U, V>& other);

	bool operator==(const scratch_allocator<T, ParentAllocator>& other) const;
	bool operator!=(const scratch_allocator<T, ParentAllocator>& other) const;

	T* allocate(std::size_t count);
	void deallocate(T* p, std::size_t count) noexcept;

	parent_allocator_type get_parent() const noexcept;

private:
	template <std::size_t StorageSize, std::size_t StorageAlignment>
	friend class scratch_pad;

	template <class U, class V>
	friend class scratch_allocator;

	scratch_allocator(sa_detail::scratch_pad_base* scratchPad, const ParentAllocator& parentAllocator);

	sa_detail::scratch_pad_base* m_scratchPad;
	parent_allocator_type m_allocator;
};
template <class T, class ParentAllocator>
template <class U, class V>
scratch_allocator<T, ParentAllocator>::scratch_allocator(const scratch_allocator<U, V>& other)
	: m_scratchPad(other.m_scratchPad)
	, m_allocator(other.m_allocator)
{
}
template <class T, class ParentAllocator>
scratch_allocator<T, ParentAllocator>::scratch_allocator(sa_detail::scratch_pad_base* scratchPad, const ParentAllocator& parentAllocator)
	: m_scratchPad(scratchPad)
	, m_allocator(parentAllocator)
{
	assert(scratchPad && "Nullptr");
}
template <class T, class ParentAllocator>
inline bool scratch_allocator<T, ParentAllocator>::operator==(const scratch_allocator<T, ParentAllocator>& other) const
{
	return m_scratchPad == other.m_scratchPad;
}
template <class T, class ParentAllocator>
inline bool scratch_allocator<T, ParentAllocator>::operator!=(const scratch_allocator<T, ParentAllocator>& other) const
{
	return !operator==(other);
}
template <class T, class ParentAllocator>
inline void scratch_allocator<T, ParentAllocator>::deallocate(T* p, std::size_t count) noexcept
{
	if (m_scratchPad->owns(p)) {
		m_scratchPad->deallocate(p, sizeof(T) * count);
	}
	else {
		m_allocator.deallocate(p, count);
	}
}
template<class T, class ParentAllocator>
inline typename scratch_allocator<T, ParentAllocator>::parent_allocator_type scratch_allocator<T, ParentAllocator>::get_parent() const noexcept
{
	return m_allocator;
}
template <class T, class ParentAllocator>
inline T* scratch_allocator<T, ParentAllocator>::allocate(std::size_t count)
{
	if (T* p = reinterpret_cast<T*>(m_scratchPad->allocate(sizeof(T) * count, alignof(T)))) {
		return p;
	}

	return m_allocator.allocate(count);
}

namespace sa_detail {
class scratch_pad_base
{
public:
	virtual void* allocate(std::size_t count, std::size_t align = 1) = 0;
	virtual void deallocate(void* p, std::size_t count) = 0;

	virtual bool owns(const void* p) const = 0;
};

constexpr std::size_t align_value(std::size_t value, std::size_t align)
{
	const std::size_t mod(value % align);
	const std::size_t multi(static_cast<bool>(mod));
	const std::size_t diff(align - mod);
	const std::size_t offset(diff * multi);

	return value + offset;
}

}

/// <summary>
/// Small pool of local memory for allocation
/// </summary>
/// <typeparam name="StorageSize">Bytes of storage</typeparam>
/// <typeparam name="StorageAlignment">Default alignment of internal storage</typeparam>
template <std::size_t StorageSize, std::size_t StorageAlignment = alignof(std::max_align_t)>
class scratch_pad : public sa_detail::scratch_pad_base
{
public:
	static constexpr std::size_t AlignedStorageSize = sa_detail::align_value(StorageSize, StorageAlignment);

	using size_type = least_unsigned_integer_t<AlignedStorageSize>;

	scratch_pad();
	~scratch_pad() noexcept;

	void* allocate(std::size_t count, std::size_t align = 1) override final;
	void deallocate(void* p, std::size_t count) override final;

	std::size_t unused_storage() const noexcept;

	bool owns(const void* p) const override final;

	void reset();

	template <class T, class ParentAllocator = std::allocator<T>>
	scratch_allocator<T, ParentAllocator> create_allocator(const ParentAllocator& parent = ParentAllocator());

private:
	struct sizes
	{
		size_type at;
		size_type used;
	};

	std::uint8_t alignas(StorageAlignment) m_bytes[AlignedStorageSize];

	std::atomic<sizes> m_sizes;
};
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline scratch_pad<StorageSize, StorageAlignment>::scratch_pad()
	: m_bytes{}
	, m_sizes({ 0, 0 })
{
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline scratch_pad<StorageSize, StorageAlignment>::~scratch_pad() noexcept
{
	assert(m_sizes.load(std::memory_order_acquire).at == 0 && "scratch_pad destroyed with items in use");
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline void* scratch_pad<StorageSize, StorageAlignment>::allocate(std::size_t count, std::size_t align)
{
	const size_type _count(static_cast<size_type>(count));
	const size_type _align(static_cast<size_type>(align));

	sizes expected(m_sizes.load(std::memory_order_acquire));
	sizes desired;
	size_type alignedBegin(0);

	do {
		const size_type begin(expected.at);
		const void* const addr(&m_bytes[begin]);
		const std::uintptr_t addrValue(reinterpret_cast<std::size_t>(addr));
		const std::uintptr_t addrAligned(sa_detail::align_value(addrValue, _align));

		const size_type offset(static_cast<size_type>(addrAligned - addrValue));
		alignedBegin = begin + offset;
		const size_type avaliable(AlignedStorageSize - alignedBegin);

		if (avaliable < _count) {
			return nullptr;
		}

		const size_type alignedEnd = alignedBegin + _count;

		desired.at = alignedEnd;
		desired.used = expected.used + _count;

	} while (!m_sizes.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed));

	return &m_bytes[alignedBegin];
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline void scratch_pad<StorageSize, StorageAlignment>::deallocate([[maybe_unused]]void* p, std::size_t count)
{
	assert(owns(p) && "Value not locally allocated");

	const size_type _count(static_cast<size_type>(count));

	sizes expected(m_sizes.load(std::memory_order_acquire));
	sizes desired;
	do {
		desired = expected;
		desired.used -= _count;

		if (desired.used == 0) {
			desired.at = 0;
		}

	} while (!m_sizes.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed));
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline std::size_t scratch_pad<StorageSize, StorageAlignment>::unused_storage() const noexcept
{
	return AlignedStorageSize - m_sizes.load(std::memory_order_acquire).at;
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline bool scratch_pad<StorageSize, StorageAlignment>::owns(const void* p) const
{
	return !(p < &m_bytes[0]) && p < &m_bytes[StorageSize];
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
inline void scratch_pad<StorageSize, StorageAlignment>::reset()
{
	m_sizes.store({ 0, 0 }, std::memory_order_release);
}
template<std::size_t StorageSize, std::size_t StorageAlignment>
template<class T, class ParentAllocator>
inline scratch_allocator<T, ParentAllocator> scratch_pad<StorageSize, StorageAlignment>::create_allocator(const ParentAllocator& parent)
{
	return scratch_allocator<T, ParentAllocator>(this, parent);
}
}
