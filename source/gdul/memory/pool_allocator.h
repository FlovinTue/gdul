#pragma once

// Copyright(c) 2020 Flovin Michaelsen
// 
// Permission is hereby granted, free of chargeo any person obtaining a copy
// of this software and associated documentation files(the "Software")o deal
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
// LIABILITY, WHETHER IN AN ACTION OF CONTRACTORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <stdint.h>
#include <memory>
#include <gdul/memory/concurrent_guard_pool.h>

namespace gdul
{
namespace pa_detail {

using size_type = std::size_t;

class memory_pool_base
{
public:
	virtual bool verify_compatibility(size_type ItemSize, size_type ItemAlign) const = 0;

	virtual void* get_block() = 0;
	virtual void recycle_block(void* block) = 0;
};

template <size_type ItemSize, size_type ItemAlign, class ParentAllocator>
class memory_pool_impl;
}

// Allocator associated with a pool instance. Is only able to request memory blocks of the 
// size that the pool is initialized with (but may do so quickly)
template <class T, bool PoolOwnership = false>
class pool_allocator
{
public:
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const pointer;
	using size_type = pa_detail::size_type;

	using pool_ptr_type = std::conditional_t<PoolOwnership, shared_ptr<pa_detail::memory_pool_base>, raw_ptr<pa_detail::memory_pool_base>>;

	template <typename U>
	struct rebind
	{
		using other = pool_allocator<U, PoolOwnership>;
	};

	template <class U>
	pool_allocator(const pool_allocator<U, PoolOwnership>& other);
	template <class U>
	pool_allocator(pool_allocator<U, PoolOwnership>&& other);

	template <class U>
	pool_allocator& operator=(const pool_allocator<U, PoolOwnership>& other);
	template <class U>
	pool_allocator& operator=(pool_allocator<U, PoolOwnership>&& other);

	value_type* allocate(size_type = 1);
	void deallocate(value_type* block, size_type = 1);

	template <class U>
	pool_allocator<U, PoolOwnership> convert() const;

	pool_allocator(const pool_ptr_type& memoryPool);
private:

	friend class memory_pool;

	pool_ptr_type m_pool;
};

template<class T, bool PoolOwnership>
inline pool_allocator<T, PoolOwnership>::pool_allocator(const pool_ptr_type& memoryPool)
	: m_pool(memoryPool)
{}
template<class T, bool PoolOwnership>
template<class U>
inline pool_allocator<T, PoolOwnership>::pool_allocator(const pool_allocator<U, PoolOwnership>& other)
{
	operator=(other);
}
template<class T, bool PoolOwnership>
template<class U>
inline pool_allocator<T, PoolOwnership>::pool_allocator(pool_allocator<U, PoolOwnership>&& other)
{
	operator=(std::move(other));
}
template<class T, bool PoolOwnership>
template <class U>
inline pool_allocator<T, PoolOwnership>& pool_allocator<T, PoolOwnership>::operator=(const pool_allocator<U, PoolOwnership>& other)
{
	m_pool = std::move(other.convert<T>().m_pool);
	return *this;
}
template<class T, bool PoolOwnership>
template <class U>
inline pool_allocator<T, PoolOwnership>& pool_allocator<T, PoolOwnership>::operator=(pool_allocator<U, PoolOwnership>&& other)
{
	operator=(other);
	return *this;
}
template<class T, bool PoolOwnership>
template<class U>
inline pool_allocator<U, PoolOwnership> pool_allocator<T, PoolOwnership>::convert() const
{
	assert(m_pool->verify_compatibility(sizeof(U), alignof(U)) && "Memory pool stats incompatible with type");
	return pool_allocator<U, PoolOwnership>(m_pool);
}
template<class T, bool PoolOwnership>
inline typename pool_allocator<T, PoolOwnership>::value_type* pool_allocator<T, PoolOwnership>::allocate(size_type)
{
	return (value_type*)m_pool->get_block();
}
template<class T, bool PoolOwnership>
inline void pool_allocator<T, PoolOwnership>::deallocate(value_type* block, size_type)
{
	m_pool->recycle_block((void*)block);
}

class memory_pool
{
public:
	memory_pool() = default;

	using size_type = pa_detail::size_type;

	/// <summary>
	/// Initialize pool
	/// </summary>
	/// <typeparam name="ItemSize">Size of items</typeparam>
	/// <typeparam name="ItemAlign">Alignment of items</typeparam>
	/// <typeparam name="ParentAllocator">Parent allocator for creating block collections</typeparam>
	/// <param name="initialCapacity">Initial blocks allocated</param>
	/// <param name="itemsPerBlock">Number of items returned on allocate</param>
	/// <param name="allocator">Parent allocator for creating block collections</param>
	template <size_type ItemSize, size_type ItemAlign, class ParentAllocator = std::allocator<std::uint8_t>>
	void init(size_type initialCapacity, size_type itemsPerBlock = 1, ParentAllocator allocator = ParentAllocator());

	/// <summary>
	/// Create an allocator associated with this pool
	/// </summary>
	/// <typeparam name="T">Type associated with allocator. Only size and alignement of type is important really -- for stl compatibility</typeparam>
	/// <typeparam name="PoolOwnership">Determines if allocator may outlive it's pool. Using false allows for cheaper allocator copying</typeparam>
	/// <returns>New allocator</returns>
	template <class T, bool PoolOwnership = false>
	pool_allocator<T, PoolOwnership> create_allocator() const;

	void reset();

private:
	shared_ptr<pa_detail::memory_pool_base> m_impl;
};
template<memory_pool::size_type ItemSize, memory_pool::size_type ItemAlign, class ParentAllocator>
inline void memory_pool::init(memory_pool::size_type initialCapacity, memory_pool::size_type itemsPerBlock, ParentAllocator allocator)
{
	if (m_impl)
		return;

	m_impl = gdul::allocate_shared<pa_detail::memory_pool_impl<ItemSize, ItemAlign, ParentAllocator>>(allocator, initialCapacity, itemsPerBlock, allocator);
}
inline void memory_pool::reset() 
{
	m_impl = shared_ptr<pa_detail::memory_pool_base>();
}
template<class T, bool PoolOwnership>
inline pool_allocator<T, PoolOwnership> memory_pool::create_allocator() const
{
	assert(m_impl && "Pool not initialized");
	assert(m_impl->verify_compatibility(sizeof(T), alignof(T)) && "Memory pool stats incompatible with type");
	return pool_allocator<T, PoolOwnership>((typename pool_allocator<T, PoolOwnership>::pool_ptr_type)m_impl);
}
namespace pa_detail {
template <size_type ItemSize, size_type ItemAlign, class ParentAllocator>
class memory_pool_impl : public memory_pool_base
{
	struct alignas(ItemAlign) item_rep
	{
		std::uint8_t m_block[ItemSize];
	};
public:
	memory_pool_impl(size_type initialCapacity, size_type itemsPerBlock, ParentAllocator allocator)
		: m_pool((typename decltype(m_pool)::size_type)initialCapacity, itemsPerBlock, std::max<size_type>((typename decltype(m_pool)::size_type)(initialCapacity / std::thread::hardware_concurrency()), 1), allocator)
	{}
	void* get_block() override final {
		return (void*)m_pool.get();
	}
	void recycle_block(void* block) override final {
		m_pool.recycle((item_rep*)block);
	}

	bool verify_compatibility(size_type otherItemSize, size_type otherItemAlign) const override final {
		constexpr size_type maxItemSize(ItemSize);
		constexpr size_type maxItemAlign(ItemAlign);
		const bool size(!(maxItemSize < otherItemSize));
		const bool align(!(maxItemAlign < otherItemAlign));
		return size && align;
	}

private:
	concurrent_guard_pool<item_rep, ParentAllocator> m_pool;
};
}

}