#pragma once

#include <functional>


// Idea for future: 
// use x byte block to construct
// abstracted-away callable. (Type erasure)
// If > size use std::function

namespace gdul {


// If jobs are to have lifetimes, then (probably an object-pool should be used)
// ...

namespace job_handler_detail {

class job_impl;
}
class job
{
public:
	job();
	job(std::function<void()>&& callable);
	// Ugh. Register dependency. Lock-free? Race conditions galore. Hmm....
	job(std::function<void()>&& callable, const job& dependency);
	job(std::function<void()>&& callable, uint8_t priority);
	job(std::function<void()>&& callable, const job& dependency, uint8_t priority);

	~job();

	job(job&&) = default;
	job(const job&) = default;
	job& operator=(job&&) = default;
	job& operator=(const job&) = default;
	
	void operator()() const;

	uint64_t get_sequence_key() const;

private:
	job_handler_detail::job_impl* myImpl;
	uint8_t priority;
};

}
