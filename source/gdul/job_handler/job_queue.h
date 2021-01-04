#pragma once

#include <gdul/job_handler/job.h>
#include <gdul/job_handler/globals.h>
#include <gdul/concurrent_priority_queue/concurrent_priority_queue.h>
#include <gdul/concurrent_queue/concurrent_queue.h>

namespace gdul {
	
class job;
namespace jh_detail {

using job_impl_shared_ptr = shared_ptr<job_impl>;
class job_impl;

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

protected:
	float priority(const jh_detail::job_impl_shared_ptr& of) const;
};

template <class Allocator = jh_detail::allocator_type>
class job_async_queue : public job_queue
{
public:

	job_async_queue() = default;
	job_async_queue(Allocator alloc);


private:
	void submit_job(jh_detail::job_impl_shared_ptr jb) override final;
	jh_detail::job_impl_shared_ptr fetch_job() override final;


	concurrent_queue<jh_detail::job_impl_shared_ptr, Allocator> m_queue;
};
template <class Allocator = jh_detail::allocator_type>
class job_sync_queue : public job_queue
{
public:
	job_sync_queue() = default;
	job_sync_queue(Allocator alloc);

private:
	void submit_job(jh_detail::job_impl_shared_ptr jb) override final;
	jh_detail::job_impl_shared_ptr fetch_job() override final;

	concurrent_priority_queue<float, jh_detail::job_impl_shared_ptr, jh_detail::Job_Pool_Init_Size, Allocator> m_queue;
};

template<class Allocator>
inline job_async_queue<Allocator>::job_async_queue(Allocator alloc)
	: m_queue(alloc)
{
}

template<class Allocator>
inline void job_async_queue<Allocator>::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::move(jb));
}
template<class Allocator>
inline jh_detail::job_impl_shared_ptr job_async_queue<Allocator>::fetch_job()
{
	jh_detail::job_impl_shared_ptr out;
	m_queue.try_pop(out);
	return out;
}

template<class Allocator>
inline job_sync_queue<Allocator>::job_sync_queue(Allocator alloc)
	: m_queue(alloc)
{
}

template<class Allocator>
inline void job_sync_queue<Allocator>::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::make_pair(this->priority(jb), std::move(jb)));
}
template<class Allocator>
inline jh_detail::job_impl_shared_ptr job_sync_queue<Allocator>::fetch_job()
{
	jh_detail::job_impl_shared_ptr out;
	m_queue.try_pop(out);
	return out;
}
}