#pragma once

// Copyright(c) 2019 Flovin Michaelsen
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

#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul
{
namespace call_detail {
class memory_chunk_pool_base
{
public:
	virtual bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const = 0;

	virtual void* get_chunk() = 0;
	virtual void recycle_chunk(void* chunk) = 0;
};

template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_chunk_pool_impl;
}

template <class T>
class chunk_allocator
{
public:
	using value_type = T;

	// raw_ptr and shared_ptr may be used here, based on if an allocator
	// is allowed to outlive its pool or not
	using pool_ptr_type = raw_ptr<call_detail::memory_chunk_pool_base>;

	template <typename U>
	struct rebind
	{
		using other = chunk_allocator<U>;
	};

	template <class U>
	chunk_allocator(const chunk_allocator<U>& other);
	template <class U>
	chunk_allocator(chunk_allocator<U>&& other);

	template <class U>
	chunk_allocator& operator=(const chunk_allocator<U>& other);
	template <class U>
	chunk_allocator& operator=(chunk_allocator<U>&& other);

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

	template <class U>
	chunk_allocator<U> convert() const;

	chunk_allocator(const pool_ptr_type& chunkPool);
private:

	friend class memory_chunk_pool;

	pool_ptr_type m_pool;
};

template<class T>
inline chunk_allocator<T>::chunk_allocator(const pool_ptr_type& chunkPool)
	: m_pool(chunkPool)
{}
template<class T>
template<class U>
inline chunk_allocator<T>::chunk_allocator(const chunk_allocator<U>& other)
{
	operator=(other);
}
template<class T>
template<class U>
inline chunk_allocator<T>::chunk_allocator(chunk_allocator<U>&& other)
{
	operator=(std::move(other));
}
template<class T>
template <class U>
inline chunk_allocator<T>& chunk_allocator<T>::operator=(const chunk_allocator<U>& other)
{
	m_pool = std::move(other.convert<T>().m_pool);
	return *this;
}
template<class T>
template <class U>
inline chunk_allocator<T>& chunk_allocator<T>::operator=(chunk_allocator<U>&& other)
{
	operator=(other);
	return *this;
}
template<class T>
template<class U>
inline chunk_allocator<U> chunk_allocator<T>::convert() const
{
	assert(m_pool->verify_compatibility(sizeof(U), alignof(U)) && "Memory pool stats incompatible with type");
	return chunk_allocator<U>(m_pool);
}
template<class T>
inline typename chunk_allocator<T>::value_type* chunk_allocator<T>::allocate(std::size_t)
{
	return (value_type*)m_pool->get_chunk();
}
template<class T>
inline void chunk_allocator<T>::deallocate(value_type* chunk, std::size_t)
{
	m_pool->recycle_chunk((void*)chunk);
}

class memory_chunk_pool
{
public:
	memory_chunk_pool() = default;

	template <std::size_t ChunkSize, std::size_t ChunkAlign>
	void init(std::size_t chunksPerBlock);

	template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
	void init(std::size_t chunksPerBlock, ParentAllocator allocator);

	void reset();

	template <class T>
	chunk_allocator<T> create_allocator() const;

private:
	shared_ptr<call_detail::memory_chunk_pool_base> m_impl;
};
template<std::size_t ChunkSize, std::size_t ChunkAlign>
inline void memory_chunk_pool::init(std::size_t chunksPerBlock)
{
	init<ChunkSize, ChunkAlign>(chunksPerBlock, std::allocator<std::uint8_t>());
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_chunk_pool::init(std::size_t chunksPerBlock, ParentAllocator allocator)
{
	if (m_impl)
		return;

	m_impl = gdul::allocate_shared<call_detail::memory_chunk_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>>(allocator, chunksPerBlock, allocator);
}
inline void memory_chunk_pool::reset() 
{
	m_impl = shared_ptr<call_detail::memory_chunk_pool_base>();
}
template<class T>
inline chunk_allocator<T> memory_chunk_pool::create_allocator() const
{
	assert(m_impl->verify_compatibility(sizeof(T), alignof(T)) && "Memory pool stats incompatible with type");
	return chunk_allocator<T>((typename chunk_allocator<T>::pool_ptr_type)m_impl);
}
namespace call_detail {
template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_chunk_pool_impl : public memory_chunk_pool_base
{
	struct alignas(ChunkAlign) chunk_rep
	{
		std::uint8_t m_block[ChunkSize];
	};
public:
	memory_chunk_pool_impl(std::size_t chunksPerBlock, ParentAllocator allocator)
		: m_pool(chunksPerBlock, allocator)
	{}
	void* get_chunk() override final {
		return (void*)m_pool.get_object();
	}
	void recycle_chunk(void* chunk) override final {
		m_pool.recycle_object((chunk_rep*)chunk);
	}

	bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const override final {
		constexpr std::size_t maxChunkSize(ChunkSize);
		constexpr std::size_t maxChunkAlign(ChunkAlign);
		const bool size(!(maxChunkSize < chunkSize));
		const bool align(!(maxChunkAlign < chunkAlign));
		return size & align;
	}

private:
	concurrent_object_pool<chunk_rep, ParentAllocator> m_pool;
};
}

}