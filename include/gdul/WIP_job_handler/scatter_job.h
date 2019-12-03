#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_delegate.h>
#include <vector>

namespace gdul
{
// Input number of splits or size of slices? 
// Create as a job... with priority? 
// Inherit from job ? this would be kind nice. Maybe? 
// From job_handler -> make_scatter_job? 
// Maybe handle behind the scenes and simply return a job that will. No. Or ? 

// Since we need to be able to fetch gather results

// What to do?. Need eval to establish if element should output. 
// ALSO. Processing
// Callable with signature bool(T& obj) ? 


template <class T>
class scatter_job
{
public:
	scatter_job(std::vector<T>& dataSource, std::vector<T>& dataTarget, job_delegate process, std::size_t jobs);

private:
	std::vector<T>& m_scatter;
	std::vector<T>& m_gather;
	
};
}