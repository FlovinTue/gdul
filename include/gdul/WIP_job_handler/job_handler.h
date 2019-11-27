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

#pragma warning(push)
#pragma warning(disable : 4324)

#include <array>

#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <gdul\concurrent_queue\concurrent_queue.h>

#include <gdul\WIP_job_handler\job.h>
#include <gdul\WIP_job_handler\job_handler_commons.h>
#include <gdul\WIP_job_handler\job_impl.h>
#include <gdul\WIP_job_handler\chunk_allocator.h>
#include <gdul\WIP_job_handler\worker_impl.h>
#include <gdul\WIP_job_handler\worker.h>

namespace gdul {

namespace jh_detail {

class job_impl;
}

class job_handler
{
public:
	using allocator_type = jh_detail::allocator_type;
	using job_impl_shared_ptr = jh_detail::job_impl::job_impl_shared_ptr;

	static thread_local job this_job;
	static thread_local worker this_worker;

	job_handler();
	job_handler(allocator_type& allocator);
	~job_handler();

 	void retire_workers();

	worker make_worker();

	// Callable will allocate if size is above jh_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	// Priority corresponds to the internal queue it will be placed in. Priority 0 -> highest. 
	// jh_detail::Priority_Granularity defines the number of queueus
	template <class Callable>
	job make_job(Callable&& callable, std::uint8_t priority);

	// Callable will allocate if size is above jh_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	template <class Callable>
	job make_job(Callable&& callable);

	std::size_t num_workers() const;
	std::size_t num_enqueued() const;
private:
	static thread_local jh_detail::worker_impl* this_worker_impl;
	static thread_local jh_detail::worker_impl ourImplicitWorker;

	friend class jh_detail::job_impl;
	friend class job;

	template <class Callable>
	job_impl_shared_ptr make_job_impl(Callable&& callable, std::uint8_t priority);

	void enqueue_job(job_impl_shared_ptr job);

	void launch_worker(std::uint16_t index);

	void work();

	job_impl_shared_ptr fetch_job();

	allocator_type m_mainAllocator;

	concurrent_object_pool<jh_detail::job_impl_chunk_rep, jh_detail::allocator_type> m_jobImplChunkPool;

	concurrent_queue<job_impl_shared_ptr, allocator_type> m_jobQueues[jh_detail::Priority_Granularity];
	
	jh_detail::chunk_allocator<jh_detail::job_impl, jh_detail::job_impl_chunk_rep> m_jobImplAllocator;

	std::array<jh_detail::worker_impl, jh_detail::Job_Handler_Max_Workers> m_workers;

	std::atomic<std::uint16_t> m_workerCount;
};

template<class Callable>
inline job job_handler::make_job(Callable && callable, std::uint8_t priority)
{
	return job(make_job_impl(std::forward<Callable&&>(callable), priority));
}

template<class Callable>
inline job job_handler::make_job(Callable && callable)
{
	return make_job(std::forward<Callable&&>(callable), jh_detail::Default_Job_Priority);
}

template<class Callable>
inline job_handler::job_impl_shared_ptr job_handler::make_job_impl(Callable&& callable, std::uint8_t priority)
{
	assert(priority < jh_detail::Priority_Granularity && "Priority value out of bounds");

	const uint8_t _priority(priority < jh_detail::Priority_Granularity ? priority : jh_detail::Priority_Granularity - 1);

	return make_shared<jh_detail::job_impl, decltype(m_jobImplAllocator)>
		(
			m_jobImplAllocator, 
			this, 
			std::forward<Callable&&>(callable), 
			_priority, 
			m_mainAllocator
			);
}

}
#pragma warning(pop)

// .. Array scatter-gather helper?
// Needs Source array.. oor.. begin -> end iterator. Probably better.
// Needs operation to perform... Hmm generalize more.. hmmm
// And an output array? ( or output begin->end iterator) 
// Use cases? Hmm. Need to generalize for more use cases?
// For example sort? How would that work?
// Culling? Output arrays would not match input.
