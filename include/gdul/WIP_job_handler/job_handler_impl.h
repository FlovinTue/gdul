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

#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <gdul/concurrent_queue/concurrent_queue.h>

#include <gdul/WIP_job_handler/job.h>
#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_impl.h>
#include <gdul/WIP_job_handler/chunk_allocator.h>
#include <gdul/WIP_job_handler/worker_impl.h>
#include <gdul/WIP_job_handler/worker.h>
#include <gdul/WIP_job_handler/job_delegate.h>
namespace gdul {

namespace  jh_detail{

class job_impl;

class job_handler_impl
{
public:
	using job_impl_shared_ptr = job_impl::job_impl_shared_ptr;

	static thread_local job this_job;
	static thread_local worker this_worker;

	job_handler_impl();
	job_handler_impl(allocator_type& allocator);
	~job_handler_impl();

 	void retire_workers();

	worker make_worker();

	job make_job(job_delegate<void, void>&& call);

	std::size_t num_workers() const noexcept;
	std::size_t num_enqueued() const noexcept;
private:
	friend class job_impl;
	friend class job;

	static thread_local worker_impl* this_worker_impl;
	static thread_local worker_impl ourImplicitWorker;

	using job_impl_allocator = chunk_allocator<job_impl, job_impl_chunk_rep>;
	using job_dependee_allocator = chunk_allocator<job_dependee, job_dependee_chunk_rep>;

	job_dependee_allocator get_job_dependee_allocator() const noexcept;

	void enqueue_job(job_impl_shared_ptr job);

	void launch_worker(std::uint16_t index) noexcept;

	void work();

	job_impl_shared_ptr fetch_job();

	allocator_type m_mainAllocator;

	concurrent_object_pool<job_impl_chunk_rep, allocator_type> m_jobImplChunkPool;
	concurrent_object_pool<job_dependee_chunk_rep, allocator_type> m_jobDependeeChunkPool;

	concurrent_queue<job_impl_shared_ptr, allocator_type> m_jobQueues[Priority_Granularity];
	
	job_impl_allocator m_jobImplAllocator;
	job_dependee_allocator m_jobDependeeAllocator;

	std::array<worker_impl, Job_Handler_Max_Workers> m_workers;

	std::atomic<std::uint16_t> m_workerCount;
};
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
