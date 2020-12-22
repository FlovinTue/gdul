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
#include <gdul/memory/concurrent_guard_pool.h>

namespace gdul
{
namespace pa_detail {

using size_type = std::size_t;

class memory_pool_base
{
public:
	virtual bool verify_compatibility(size_type chunkSize, size_type chunkAlign) const = 0;

	virtual void* get_chunk() = 0;
	virtual void recycle_chunk(void* chunk) = 0;
};

template <size_type ChunkSize, size_type ChunkAlign, class ParentAllocator>
class memory_pool_impl;
}

// Allocator associated with a pool instance. Is only able to request memory chunks of the 
// size that the pool is initialized with (but may do so quickly)
template <class T>
class pool_allocator
{
public:
	using value_type = T;
	using pointer = value_type*;
	using const_pointer = const pointer;
	using size_type = pa_detail::size_type;

	// raw_ptr and shared_ptr may be used here, based on if an allocator
	// is allowed to outlive its pool or not
	using pool_ptr_type = raw_ptr<pa_detail::memory_pool_base>;

	template <typename U>
	struct rebind
	{
		using other = pool_allocator<U>;
	};

	template <class U>
	pool_allocator(const pool_allocator<U>& other);
	template <class U>
	pool_allocator(pool_allocator<U>&& other);

	template <class U>
	pool_allocator& operator=(const pool_allocator<U>& other);
	template <class U>
	pool_allocator& operator=(pool_allocator<U>&& other);

	value_type* allocate(size_type);
	void deallocate(value_type* chunk, size_type);

	template <class U>
	pool_allocator<U> convert() const;

	pool_allocator(const pool_ptr_type& memoryPool);
private:

	friend class memory_pool;

	pool_ptr_type m_pool;
};

template<class T>
inline pool_allocator<T>::pool_allocator(const pool_ptr_type& memoryPool)
	: m_pool(memoryPool)
{}
template<class T>
template<class U>
inline pool_allocator<T>::pool_allocator(const pool_allocator<U>& other)
{
	operator=(other);
}
template<class T>
template<class U>
inline pool_allocator<T>::pool_allocator(pool_allocator<U>&& other)
{
	operator=(std::move(other));
}
template<class T>
template <class U>
inline pool_allocator<T>& pool_allocator<T>::operator=(const pool_allocator<U>& other)
{
	m_pool = std::move(other.convert<T>().m_pool);
	return *this;
}
template<class T>
template <class U>
inline pool_allocator<T>& pool_allocator<T>::operator=(pool_allocator<U>&& other)
{
	operator=(other);
	return *this;
}
template<class T>
template<class U>
inline pool_allocator<U> pool_allocator<T>::convert() const
{
	assert(m_pool->verify_compatibility(sizeof(U), alignof(U)) && "Memory pool stats incompatible with type");
	return pool_allocator<U>(m_pool);
}
template<class T>
inline typename pool_allocator<T>::value_type* pool_allocator<T>::allocate(size_type)
{
	return (value_type*)m_pool->get_chunk();
}
template<class T>
inline void pool_allocator<T>::deallocate(value_type* chunk, size_type)
{
	m_pool->recycle_chunk((void*)chunk);
}

class memory_pool
{
public:
	memory_pool() = default;

	using size_type = pa_detail::size_type;

	template <size_type ChunkSize, size_type ChunkAlign>
	void init(size_type chunksPerBlock);

	template <size_type ChunkSize, size_type ChunkAlign, class ParentAllocator>
	void init(size_type chunksPerBlock, ParentAllocator allocator);

	void reset();

	template <class T>
	pool_allocator<T> create_allocator() const;

private:
	shared_ptr<pa_detail::memory_pool_base> m_impl;
};
template<memory_pool::size_type ChunkSize, memory_pool::size_type ChunkAlign>
inline void memory_pool::init(size_type chunksPerBlock)
{
	init<ChunkSize, ChunkAlign>(chunksPerBlock, std::allocator<std::uint8_t>());
}
template<memory_pool::size_type ChunkSize, memory_pool::size_type ChunkAlign, class ParentAllocator>
inline void memory_pool::init(size_type chunksPerBlock, ParentAllocator allocator)
{
	if (m_impl)
		return;

	m_impl = gdul::allocate_shared<pa_detail::memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>>(allocator, chunksPerBlock, allocator);
}
inline void memory_pool::reset() 
{
	m_impl = shared_ptr<pa_detail::memory_pool_base>();
}
template<class T>
inline pool_allocator<T> memory_pool::create_allocator() const
{
	assert(m_impl->verify_compatibility(sizeof(T), alignof(T)) && "Memory pool stats incompatible with type");
	return pool_allocator<T>((typename pool_allocator<T>::pool_ptr_type)m_impl);
}
namespace pa_detail {
template <size_type ChunkSize, size_type ChunkAlign, class ParentAllocator>
class memory_pool_impl : public memory_pool_base
{
	struct alignas(ChunkAlign) chunk_rep
	{
		std::uint8_t m_block[ChunkSize];
	};
public:
	memory_pool_impl(size_type chunksPerBlock, ParentAllocator allocator)
		: m_pool((typename decltype(m_pool)::size_type)chunksPerBlock, (typename decltype(m_pool)::size_type)(chunksPerBlock / std::thread::hardware_concurrency()), allocator)
	{}
	void* get_chunk() override final {
		return (void*)m_pool.get();
	}
	void recycle_chunk(void* chunk) override final {
		m_pool.recycle((chunk_rep*)chunk);
	}

	bool verify_compatibility(size_type chunkSize, size_type chunkAlign) const override final {
		constexpr size_type maxChunkSize(ChunkSize);
		constexpr size_type maxChunkAlign(ChunkAlign);
		const bool size(!(maxChunkSize < chunkSize));
		const bool align(!(maxChunkAlign < chunkAlign));
		return size & align;
	}

private:
	concurrent_guard_pool<chunk_rep, ParentAllocator> m_pool;
};
}

}