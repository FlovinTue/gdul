#pragma once

#include <gdul/job_handler/job.h>
#include <gdul/job_handler/globals.h>
#include <gdul/concurrent_priority_queue/concurrent_priority_queue.h>
#include <gdul/concurrent_queue/concurrent_queue.h>

namespace gdul {
	
class job;
namespace jh_detail {
class job_impl;
using job_impl_shared_ptr = shared_ptr<job_impl>;
}

class job_queue
{
public:
	virtual ~job_queue() = default;

private:
	friend class job;
	friend class jh_detail::job_impl;
	friend class jh_detail::worker_impl;

	virtual jh_detail::job_impl_shared_ptr fetch_job() = 0;
	virtual void submit_job(jh_detail::job_impl_shared_ptr jb) = 0;
};

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
class job_sync_queue : public job_queue
{
public:
	job_sync_queue() = default;
	job_sync_queue(jh_detail::allocator_type alloc);

private:
	void submit_job(jh_detail::job_impl_shared_ptr jb) override final;
	jh_detail::job_impl_shared_ptr fetch_job() override final;

	concurrent_priority_queue<float, jh_detail::job_impl_shared_ptr, jh_detail::Job_Pool_Init_Size, cpq_allocation_strategy_pool<jh_detail::allocator_type>> m_queue;
};
}