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

#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/job_handler/globals.h>
#include <gdul/concurrent_priority_queue/concurrent_priority_queue.h>
#include <gdul/concurrent_queue/concurrent_queue.h>

namespace gdul {
	
class job;
class job_handler;
namespace jh_detail {
class job_impl;
using job_impl_shared_ptr = shared_ptr<job_impl>;
}

/// <summary>
/// Job queue interface
/// </summary>
class job_queue
{
public:
	virtual ~job_queue() = default;

	std::uint8_t assigned_workers() const;
private:
	friend class job;
	friend class jh_detail::job_impl;
	friend class jh_detail::worker_impl;

	virtual jh_detail::job_impl_shared_ptr fetch_job() = 0;
	virtual void submit_job(jh_detail::job_impl_shared_ptr jb) = 0;

	std::atomic_uint8_t m_assignees = 0;
};

/// <summary>
/// Basic queue intended for asynchronous jobs. The underlying queue is a relaxed FIFO variant
/// which does not preserve ordering between producers
/// </summary>
class job_async_queue : public job_queue
{
public:

	job_async_queue() = default;
	job_async_queue(jh_detail::allocator_type alloc);


private:
	void submit_job(jh_detail::job_impl_shared_ptr jb) override final;
	jh_detail::job_impl_shared_ptr fetch_job() override final;


	concurrent_queue<jh_detail::job_impl_shared_ptr, jh_detail::allocator_type> m_queue;
};

/// <summary>
/// Priority queue for use with jobs occuring within the span of one frame. Jobs will be reordered to promote
/// the maximum amount of parallelism
/// </summary>
class job_sync_queue : public job_queue
{
public:
	job_sync_queue() = default;
	job_sync_queue(jh_detail::allocator_type alloc);

private:
	void submit_job(jh_detail::job_impl_shared_ptr jb) override final;
	jh_detail::job_impl_shared_ptr fetch_job() override final;

	concurrent_priority_queue<float, jh_detail::job_impl_shared_ptr, jh_detail::Job_Pool_Init_Size, cpq_allocation_strategy_pool<jh_detail::allocator_type>, std::greater<float>> m_queue;
};
}