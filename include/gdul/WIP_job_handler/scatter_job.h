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
	scatter_job(const std::vector<T>& dataSource, std::vector<T*>& dataTarget, std::size_t batchSize, job_delegate process);

	using input_vector_type = std::vector<T>;
	using output_vector_type = std::vector<T*>;

private:
	void prepare_output();
	void work_func(std::uint32_t begin, std::uint32_t end);
	void enqueue_jobs();
	void collect();

	const job_delegate m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;
	const std::size_t m_batchSize;
	const std::size_t m_batches;
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
	std::vector<T>::reverse_iterator rIter(m_output.rbegin());
	std::uintptr_t collected((std::uintptr_t)(*rIter));

	for (std::size_t i = 1; i < m_batches; ++i) {
		std::size_t batchIndex(i * m_batchSize);
		++rIter;
		std::uintptr_t next((std::uintptr_t)(*rIter));

		std::vector<T>::iterator first(m_output.begin() + batchIndex);
		std::vector<T>::iterator second(m_output.begin() + (batchIndex + next));

		std::copy(first, second, m_output.begin() + collected);

		collected += next;
	}

	m_output.resize(collected);
}
template<class T>
inline void scatter_job<T>::prepare_output()
{
	m_output.resize(m_input.size() + m_chunks);
	std::uninitialized_fill(m_output.begin(), m_output.end(), nullptr);
}
template<class T>
inline void scatter_job<T>::work_func(std::uint32_t begin, std::uint32_t end)
{
	std::uintptr_t done(0);

	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_input[i])){
			++done;
			m_output[begin + done] = m_input[i];
		}
	}

	const std::size_t batch((std::size_t)begin / m_batchSize);

	output_vector_type::reverse_iterator rIter(m_output.rend() + batch);

	*rIter = (T*)done;
}
}