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

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul
{
template <class Object, class Allocator>
class concurrent_object_pool;

namespace job_handler_detail
{
// Defined in job_impl.h
struct alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) job_impl_chunk_rep;

// Wrapper to get / recycle chunks from object pool
template <class Dummy>
class job_impl_allocator
{
public:
	using value_type = Dummy;
	using allocator_type = job_handler_detail::allocator_type;

	template <typename U>
	struct rebind
	{
		using other = job_impl_allocator<U>;
	};


	job_impl_allocator(concurrent_object_pool<job_impl_chunk_rep, allocator_type>* chunkSrc);

	template <class Dummy_>
	job_impl_allocator(const job_impl_allocator<Dummy_>& other);

	value_type* allocate(std::size_t);
	void deallocate(value_type* chunk, std::size_t);

	concurrent_object_pool<job_impl_chunk_rep, allocator_type>* m_chunks;
};

template<class Dummy>
inline job_impl_allocator<Dummy>::job_impl_allocator(concurrent_object_pool<job_impl_chunk_rep, allocator_type>* chunkSrc)
	: m_chunks(chunkSrc)
{
}
template<class Dummy>
inline typename job_impl_allocator<Dummy>::value_type* job_impl_allocator<Dummy>::allocate(std::size_t)
{
	return (value_type*)m_chunks->get_object();
}
template<class Dummy>
inline void job_impl_allocator<Dummy>::deallocate(value_type* chunk, std::size_t)
{
	m_chunks->recycle_object((job_impl_chunk_rep*)chunk);
}
template<class Dummy>
template<class Dummy_>
inline job_impl_allocator<Dummy>::job_impl_allocator(const job_impl_allocator<Dummy_>& other)
	: m_chunks(other.m_chunks)
{
}
}
}