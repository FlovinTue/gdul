#pragma once

#include <functional>
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
	job(std::function<void()>&& callable);
	// If jobs are to have lifetimes, then (probably an object-pool should be used)
	// Ugh. Register dependency. Lock-free? Race conditions galore. Hmm.... Maybe use shared_ptr, coupled with object-pool(chunk) allocation
	job(std::function<void()>&& callable, const job& dependency);
	job(std::function<void()>&& callable, std::uint8_t priority);
	job(std::function<void()>&& callable, const job& dependency, std::uint8_t priority);

	~job();

	job(job&&) = default;
	job(const job&) = default;
	job& operator=(job&&) = default;
	job& operator=(const job&) = default;
	
	void operator()() const;

	uint64_t get_sequence_key() const;

private:
	atomic_shared_ptr<job_handler_detail::job_impl> myImpl;
};

}
