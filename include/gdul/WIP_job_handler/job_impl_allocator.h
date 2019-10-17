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

#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul
{
namespace job_handler_detail
{
constexpr std::size_t Job_Impl_Chunk_Size(shared_ptr<gdul::job_handler_detail::job_impl, allocator_type>::alloc_size_make_shared());

struct alignas(log2align(Callable_Max_Size_No_Heap_Alloc)) job_impl_chunk_rep { 
	operator uint8_t*()
	{
		return reinterpret_cast<uint8_t*>(this);
	}
	uint8_t dummy[64]; 
};

template <class Dummy>
class job_impl_allocator
{
public:
	using value_type = std::uint8_t;
	using allocator_type = job_handler_detail::allocator_type;


	job_impl_allocator(concurrent_object_pool<job_impl_chunk_rep, allocator_type>* chunkSrc);

	uint8_t* allocate(std::size_t);
	void deallocate(uint8_t* block, std::size_t);

private:
	concurrent_object_pool<job_impl_chunk_rep, allocator_type>* myChunkSrc;
};

template<class Dummy>
inline job_impl_allocator<Dummy>::job_impl_allocator(concurrent_object_pool<job_impl_chunk_rep, allocator_type>* chunkSrc)
	: myChunkSrc(chunkSrc)
{
}

template<class Dummy>
inline uint8_t * job_impl_allocator<Dummy>::allocate(std::size_t)
{
	return myChunkSrc->get_object();
}
template<class Dummy>
inline void job_impl_allocator<Dummy>::deallocate(uint8_t * block, std::size_t)
{
	myChunkSrc->recycle_object(block);
}
}
}