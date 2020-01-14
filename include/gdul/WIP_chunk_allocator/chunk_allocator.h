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

// A wrapper allocator that forwards allocate / deallocate to a chunk pool. ChunkPool is
// required to implement void* get_chunk() and recycle_chunk(void*)
template <class T, class ChunkPool>
class chunk_allocator
{
public:
	using value_type = T;
	using pool_type = ChunkPool;

	template <typename U>
	struct rebind
	{
		using other = chunk_allocator<U, pool_type>;
	};

	chunk_allocator(pool_type& chunkPool);

	template <class U>
	chunk_allocator(const chunk_allocator<U, pool_type>& other);

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

	pool_type* get_pool() const;

private:
	pool_type* m_chunks;
};

template<class T, class ChunkPool>
inline chunk_allocator<T, ChunkPool>::chunk_allocator(pool_type& chunkPool)
	: m_chunks(&chunkPool)
{}
template<class T, class ChunkPool>
inline typename chunk_allocator<T, ChunkPool>::value_type* chunk_allocator<T, ChunkPool>::allocate(std::size_t)
{
	return (value_type*)m_chunks->get_chunk();
}
template<class T, class ChunkPool>
inline void chunk_allocator<T, ChunkPool>::deallocate(value_type* chunk, std::size_t)
{
	m_chunks->recycle_chunk((void*)chunk);
}
template<class T, class ChunkPool>
inline typename chunk_allocator<T, ChunkPool>::pool_type * chunk_allocator<T, ChunkPool>::get_pool() const
{
	return m_chunks;
}
template<class T, class ChunkPool>
template<class U>
inline chunk_allocator<T, ChunkPool>::chunk_allocator(const chunk_allocator<U, ChunkPool>& other)
	: m_chunks(other.get_pool())
{}

// An appropriate chunk pool for use with gdul::allocate_shared
template <class T, class Allocator = std::allocator<T>>
class allocate_shared_chunk_pool
{
	static constexpr std::size_t Chunk_Rep_Size = allocate_shared_size<T, Allocator>();
	struct alignas(alignof(T)) chunk_rep
	{
		std::uint8_t m_block[Chunk_Rep_Size];
	};

public:
	allocate_shared_chunk_pool(std::size_t chunksPerBlock)
		: m_chunks(chunksPerBlock)
	{}
	allocate_shared_chunk_pool(std::size_t chunksPerBlock, Allocator allocator)
		: m_chunks(chunksPerBlock, allocator)
	{}
	void* get_chunk(){
		return (void*)m_chunks.get_object();
	}
	void recycle_chunk(void* chunk){
		m_chunks.recycle_object((chunk_rep*)chunk);
	}

private:
	concurrent_object_pool<chunk_rep, Allocator> m_chunks;
};
}