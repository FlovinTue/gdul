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

namespace gdul
{
namespace call_detail {
class memory_pool_base
{
public:
	virtual bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const = 0;

	virtual void* get_chunk() = 0;
	virtual void recycle_chunk(void* chunk) = 0;
};

template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_pool_impl;
}

// Allocator associated with a pool instance. Is only able to request memory chunks of the 
// size that the pool is initialized with (but may do so quickly)
template <class T>
class pool_allocator
{
public:
	using value_type = T;

	// raw_ptr and shared_ptr may be used here, based on if an allocator
	// is allowed to outlive its pool or not
	using pool_ptr_type = raw_ptr<call_detail::memory_pool_base>;

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

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

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
inline typename pool_allocator<T>::value_type* pool_allocator<T>::allocate(std::size_t)
{
	return (value_type*)m_pool->get_chunk();
}
template<class T>
inline void pool_allocator<T>::deallocate(value_type* chunk, std::size_t)
{
	m_pool->recycle_chunk((void*)chunk);
}

class memory_pool
{
public:
	memory_pool() = default;

	template <std::size_t ChunkSize, std::size_t ChunkAlign>
	void init(std::size_t chunksPerBlock);

	template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
	void init(std::size_t chunksPerBlock, ParentAllocator allocator);

	void reset();

	template <class T>
	pool_allocator<T> create_allocator() const;

private:
	shared_ptr<call_detail::memory_pool_base> m_impl;
};
template<std::size_t ChunkSize, std::size_t ChunkAlign>
inline void memory_pool::init(std::size_t chunksPerBlock)
{
	init<ChunkSize, ChunkAlign>(chunksPerBlock, std::allocator<std::uint8_t>());
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool::init(std::size_t chunksPerBlock, ParentAllocator allocator)
{
	if (m_impl)
		return;

	m_impl = gdul::allocate_shared<call_detail::memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>>(allocator, chunksPerBlock, allocator);
}
inline void memory_pool::reset()
{
	m_impl = shared_ptr<call_detail::memory_pool_base>();
}
template<class T>
inline pool_allocator<T> memory_pool::create_allocator() const
{
	assert(m_impl->verify_compatibility(sizeof(T), alignof(T)) && "Memory pool stats incompatible with type");
	return pool_allocator<T>((typename pool_allocator<T>::pool_ptr_type)m_impl);
}
namespace call_detail {


static constexpr std::size_t Memory_Pool_Max_Growth = 16;
static constexpr std::size_t Memory_Pool_Min_Size = 4;

template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_pool_impl : public memory_pool_base
{
	struct alignas(ChunkAlign) chunk_rep
	{
		std::uint8_t m_block[ChunkSize]{};
		std::atomic_flag m_claim{ ATOMIC_FLAG_INIT };
	};

	struct block_rep
	{
		atomic_shared_ptr<chunk_rep[]> m_ownedBlock;
		const chunk_rep* m_begin = nullptr;
		const chunk_rep* m_end = nullptr;
		std::atomic_uint m_accessIndex = 0;
	};

public:
	memory_pool_impl(std::size_t initChunkCount, ParentAllocator allocator)
	{}
	void* get_chunk() override final {
	}
	void recycle_chunk(void* chunk) override final {
	}

	bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const override final {
		constexpr std::size_t maxChunkSize(ChunkSize);
		constexpr std::size_t maxChunkAlign(ChunkAlign);
		const bool size(!(maxChunkSize < chunkSize));
		const bool align(!(maxChunkAlign < chunkAlign));
		return size & align;
	}

	static constexpr std::size_t to_index(std::size_t chunkCount);
	static constexpr std::size_t to_chunk_count(std::size_t index);

	void try_allocate_block(std::size_t index);

private:
	std::array<block_rep, Memory_Pool_Max_Growth> m_blocks;



	ParentAllocator m_allocator;

};
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::try_allocate_block(std::size_t index)
{
	if (m_blocks[index].m_ownedBlock.get_version() != 0) {
		return;
	}

	raw_ptr<chunk_rep[]> expected(nullptr, 0);

	shared_ptr<chunk_rep[]> desired(make_shared<chunk_rep[]>(to_chunk_count(index, m_allocator));

	m_blocks[index].m_ownedBlock.compare_exchange_strong(expected, desired);
}
}

}