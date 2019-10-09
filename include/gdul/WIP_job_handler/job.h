#pragma once

#include <functional>


namespace gdul {

class job
{
public:
	job();
	job(std::function<void()>&& callable);
	job(std::function<void()>&& callable, const job& dependency);

	~job();

	job(job&&) = default;
	job(const job&) = default;
	job& operator=(job&&) = default;
	job& operator=(const job&) = default;
	
	void operator()() const;

private:
	std::function<void()> myWorkItem;
	uint64_t myPriorityIndex;
};

}
