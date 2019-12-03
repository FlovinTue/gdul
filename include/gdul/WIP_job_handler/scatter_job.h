#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_delegate.h>
#include <gdul/WIP_job_handler/job_handler.h>
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
	scatter_job(std::vector<T>& dataSource, std::vector<T>& dataTarget, std::size_t chunkSize, job_delegate process);

private:
	void prepare_data();
	void work_func(std::uint32_t begin, std::uint32_t end);
	void enqueue_jobs();
	void collect();

	std::vector<T>& m_scatter;
	std::vector<T*>& m_gather;
	job_delegate m_process;
};

template<class T>
inline void scatter_job<T>::enqueue_jobs()
{
	std::uint32_t begin;
	std::uint32_t end;
	job newjob(job_handler::make_job(&scatter_job::work_func, this, begin, end));
}
template<class T>
inline void scatter_job<T>::collect()
{
	std::vector<T>::iterator fIter(m_gather.begin());
	std::vector<T>::reverse_iterator rIter(m_gather.end());

	for (; fIter != rIter; ++fIter){
		if (*fIter){
			continue;
		}
		if (*rIter){
			std::swap(*fIter, *rIter);
		}

		--rIter;
	}
}
template<class T>
inline void scatter_job<T>::prepare_data()
{
	m_gather.resize(m_scatter.size());
	std::uninitialized_fill(m_gather.begin(), m_gather.end(), nullptr);
}
template<class T>
inline void scatter_job<T>::work_func(std::uint32_t begin, std::uint32_t end)
{
	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_scatter[i])){
			m_gather[i] = &m_scatter[i];
		}
	}
}
}