// Copyright(c) 2019 Flovin Michaelsen
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

#include <gdul\job_handler\job_handler_utility.h>

namespace gdul
{
template <class Object, class Allocator>
class concurrent_object_pool;

namespace jh_detail
{

// Wrapper to get / recycle chunks from object pool
template <class T, class ChunkRep>
class chunk_allocator
{
public:
	using value_type = T;
	using allocator_type = jh_detail::allocator_type;

	template <typename U>
	struct rebind
	{
		using other = chunk_allocator<U, ChunkRep>;
	};

	chunk_allocator(concurrent_object_pool<ChunkRep, allocator_type>* chunkSrc);

	template <class U>
	chunk_allocator(const chunk_allocator<U, ChunkRep>& other);

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

	concurrent_object_pool<ChunkRep, allocator_type>* m_chunks;
};

template<class T, class ChunkRep>
inline chunk_allocator<T, ChunkRep>::chunk_allocator(concurrent_object_pool<ChunkRep, allocator_type>* chunkSrc)
	: m_chunks(chunkSrc)
{
}
template<class T, class ChunkRep>
inline typename chunk_allocator<T, ChunkRep>::value_type* chunk_allocator<T, ChunkRep>::allocate(std::size_t)
{
	return (value_type*)m_chunks->get_object();
}
template<class T, class ChunkRep>
inline void chunk_allocator<T, ChunkRep>::deallocate(value_type* chunk, std::size_t)
{
	m_chunks->recycle_object((ChunkRep*)chunk);
}
template<class T, class ChunkRep>
template<class U>
inline chunk_allocator<T, ChunkRep>::chunk_allocator(const chunk_allocator<U, ChunkRep>& other)
	: m_chunks(other.m_chunks)
{
}
}
}