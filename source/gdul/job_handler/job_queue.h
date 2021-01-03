#pragma once

#include <gdul/job_handler/job.h>
#include <gdul/job_handler/globals.h>
#include <gdul/concurrent_priority_queue/concurrent_priority_queue.h>
#include <gdul/concurrent_queue/concurrent_queue.h>

namespace gdul {
	
class job;
namespace jh_detail {

struct job_queue
{
	virtual ~job_queue() = default;
	virtual void submit_job(const job& jb) = 0;
};
}

template <class Allocator = jh_detail::allocator_type>
struct job_async_queue : public jh_detail::job_queue
{
	job_async_queue() = default;
	job_async_queue(Allocator alloc);

	void submit_job(job jb) override final;

private:
	concurrent_queue<job, Allocator> m_queue;
};
template <class Allocator = jh_detail::allocator_type>
struct job_sync_queue : public jh_detail::job_queue
{
	job_sync_queue() = default;
	job_sync_queue(Allocator alloc);

	void submit_job(job jb) override final;

private:
	concurrent_priority_queue<float, job, jh_detail::Job_Pool_Init_Size, Allocator> m_queue;
};

template<class Allocator>
inline job_async_queue<Allocator>::job_async_queue(Allocator alloc)
	: m_queue(alloc)
{
}

template<class Allocator>
inline void job_async_queue<Allocator>::submit_job(job jb)
{
	m_queue.push(std::move(jb));
}

template<class Allocator>
inline job_sync_queue<Allocator>::job_sync_queue(Allocator alloc)
	: m_queue(alloc)
{
}

template<class Allocator>
inline void job_sync_queue<Allocator>::submit_job(job jb)
{
	m_queue.push(std::make_pair(jb.priority(), std::move(jb)));
}

}