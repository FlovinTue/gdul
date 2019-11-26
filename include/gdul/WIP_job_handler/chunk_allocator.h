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

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/chunk_pool.h>

namespace gdul
{

namespace job_handler_detail
{
// Wrapper to get / recycle chunks from pool
template <class T, class ChunkPoolType>
class chunk_allocator
{
public:
	using value_type = T;
	using allocator_type = job_handler_detail::allocator_type;

	template <typename U>
	struct rebind
	{
		using other = chunk_allocator<U>;
	};


	chunk_allocator(ChunkPoolType* pool);

	template <class U>
	chunk_allocator(const chunk_allocator<U, ChunkPoolType>& other);

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

	ChunkPoolType* m_chunks;
};

template<class T, class ChunkPoolType>
inline chunk_allocator<T, ChunkPoolType>::chunk_allocator(ChunkPoolType* chunkSrc)
	: m_chunks(chunkSrc)
{
}
template<class T, class ChunkPoolType>
inline typename chunk_allocator<T, ChunkPoolType>::value_type* chunk_allocator<T, ChunkPoolType>::allocate(std::size_t)
{
	return (value_type*)m_chunks->get_object();
}
template<class T, class ChunkPoolType>
inline void chunk_allocator<T, ChunkPoolType>::deallocate(value_type* chunk, std::size_t)
{
	m_chunks->recycle_object((ChunkType*)chunk);
}
template<class T, class ChunkPoolType>
template<class U>
inline chunk_allocator<T, ChunkPoolType>::chunk_allocator(const chunk_allocator<U, ChunkPoolType>& other)
	: m_chunks(other.m_chunks)
{
}
}
}