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

#include <gdul/memory/pool_allocator.h>
#include <gdul/delegate/delegate.h>
#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/job_handler/job/job_impl.h>
#include <gdul/execution/job_handler/job/batch_job_impl.h>
#include <gdul/execution/job_handler/worker/worker_impl.h>
#include <gdul/execution/job_handler/worker/worker.h>
#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/execution/job_handler/job/job_node.h>
#include <gdul/execution/job_handler/tracking/job_graph.h>

namespace gdul {

class job_queue;
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

 	void shutdown();

	worker make_worker();

#if defined (GDUL_JOB_DEBUG)
	job make_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t physicalId, std::size_t variationId, const char* name, const char* file, std::uint32_t line);
	job make_sub_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t batchId, std::size_t variationId, const char* name);
#else
	job make_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t physicalId, std::size_t variationId);
	job make_sub_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t batchId, std::size_t variationId);
#endif
	std::size_t worker_count() const noexcept;

	job_graph& get_job_graph();

	pool_allocator<std::uint8_t> get_job_node_allocator() const noexcept;
	pool_allocator<std::uint8_t> get_batch_job_allocator() const noexcept;

#if defined(GDUL_JOB_DEBUG)
	void dump_job_graph(const char* location);
	void dump_job_time_sets(const char* location);
#endif

private:
	void launch_worker(std::uint16_t index) noexcept;

	memory_pool m_jobImplMemPool;
	memory_pool m_jobNodeMemPool;
	memory_pool m_batchJobMemPool;

	job_graph m_jobGraph;

	std::array<worker_impl, Max_Workers> m_workers;

	std::atomic<std::uint16_t> m_workerIndices;

	allocator_type m_mainAllocator;
};
}
}
#pragma warning(pop)
