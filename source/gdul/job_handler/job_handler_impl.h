// Copyright(c) 2020 Flovin Michaelsen
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

#include <gdul/concurrent_queue/concurrent_queue.h>

#include <gdul/job_handler/job.h>
#include <gdul/job_handler/job_handler_utility.h>
#include <gdul/job_handler/job_impl.h>
#include <gdul/memory/pool_allocator.h>
#include <gdul/job_handler/worker_impl.h>
#include <gdul/job_handler/worker.h>
#include <gdul/delegate/delegate.h>
#include <gdul/job_handler/batch_job_impl.h>

namespace gdul {

namespace  jh_detail{

class job_impl;

class job_handler_impl
{
public:
	using job_impl_shared_ptr = job_impl::job_impl_shared_ptr;

	struct tl_container
	{
		worker_impl* this_worker_impl;
		worker_impl m_implicitWorker;
	}static thread_local t_items;

	job_handler_impl();
	job_handler_impl(allocator_type allocator);
	~job_handler_impl();

 	void retire_workers();

	worker make_worker();
	worker make_worker(gdul::delegate<void()> entryPoint);

	job make_job(delegate<void()>&& workUnit);

	std::size_t internal_worker_count() const noexcept;
	std::size_t external_worker_count() const noexcept;
	std::size_t active_job_count() const noexcept;

	pool_allocator<job_node> get_job_node_allocator() const noexcept;
	pool_allocator<dummy_batch_type> get_batch_job_allocator() const noexcept;

	void enqueue_job(job_impl_shared_ptr job);

	bool try_consume_from_once(job_queue consumeFrom);

private:

	void launch_worker(std::uint16_t index) noexcept;

	void work();
	void consume_job(job&& jb);

	job_impl_shared_ptr fetch_job();

	allocator_type m_mainAllocator;

	memory_pool m_jobImplMemPool;
	memory_pool m_jobNodeMemPool;
	memory_pool m_batchJobMemPool;

	concurrent_queue<job_impl_shared_ptr, allocator_type> m_jobQueues[job_queue_count];

	std::array<worker_impl, Max_Workers> m_workers;

	std::atomic<std::uint16_t> m_workerCount;
	std::atomic<std::uint16_t> m_workerIndices;
};
}
}
#pragma warning(pop)
