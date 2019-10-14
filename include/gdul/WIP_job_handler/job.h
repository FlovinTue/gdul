#pragma once

#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>

namespace gdul {

// Jobs are thread-safe to depend on?
// Thread A job, depends on thread B job. Needs atomic_shared_ptr... 
// Depend on job, or depend on job_impl ? 
// Depend on job, and return job_impl to pool?

namespace job_handler_detail {

class job_impl;
}
class job
{
public:
	job();

	template <class Callable>
	job(Callable&& callable);
	template <class Callable>
	job(Callable&& callable, const job& dependency);
	template <class Callable>
	job(Callable&& callable, std::uint8_t priority);
	template <class Callable>
	job(Callable&& callable, const job& dependency, std::uint8_t priority);

	~job();

	job(job&& other);
	job& operator=(job&& other);

	job(const job&) = delete;
	job& operator=(const job&) = delete;

private:
	atomic_shared_ptr<job_handler_detail::job_impl> myImpl;
};

}
