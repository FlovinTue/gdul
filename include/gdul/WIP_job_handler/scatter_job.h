#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_delegate.h>
#include <gdul/WIP_job_handler/job_handler.h>
#include <gdul/WIP_job_handler/job.h>
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

// Output vector adaptive? 
// Will be output
// Store counter at the end of each batch? 
// And keep propagating the 'new end' every time a pack happens? << sounds nice.

// Still need to have some ideas as to the macro handling of scatter jobs. Also need to figure out how to deal with 
// boolean return values from delegate.
template <class T>
class scatter_job : public job
{
public:
	using value_type = T;
	using deref_value_type = std::remove_pointer_t<T>;
	using input_vector_type = std::vector<value_type>;
	using output_vector_type = std::vector<deref_value_type>;

	scatter_job(const input_vector_type& dataSource, output_vector_type& dataTarget, std::size_t batchSize, job_delegate process);


private:
	void finalize();

	void prepare_output();
	void make_jobs();
	job make_process_job();
	void pack_batch(std::size_t batch);
	void process_batch(std::size_t begin, std::size_t end);

	const job_delegate m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;
	const std::size_t m_batchSize;
	const std::size_t m_batches;
};

template<class T>
inline scatter_job<T>::scatter_job(const input_vector_type& dataSource, output_vector_type& dataTarget, std::size_t batchSize, job_delegate process)
	: m_process(process)
	, m_input(dataSource)
	, m_output(dataTarget)
	, m_batchSize(batchSize)
	, m_batches(m_input.size() / batchSize + ((bool)m_input.size() % batchSize))
{
}
template<class T>
inline void scatter_job<T>::make_jobs()
{
	job first(make_process_job());

	if (1 < m_batches){
		job second;

	}

	for (std::size_t i = 1; i < m_batches; ++i){
		job next(make_process_job());

	}

}
template<class T>
inline job scatter_job<T>::make_process_job()
{
	std::size_t begin(i * m_batchSize);
	std::size_t end(begin + m_batchSize < m_input.size() ? begin + m_batchSize : m_input.size());

	job newjob(job_handler::make_job(&scatter_job::process_batch, this, begin, end));

	return newJob;
}
template<class T>
inline void scatter_job<T>::pack_batch(std::size_t batch)
{
	std::vector<T>::reverse_iterator rIter(m_output.rbegin());
	std::uintptr_t gathered((std::uintptr_t)(*rIter));

	for (std::size_t i = 1; i < m_batches; ++i) {
		std::size_t batchIndex(i * m_batchSize);
		++rIter;
		std::uintptr_t next((std::uintptr_t)(*rIter));

		std::vector<T>::iterator first(m_output.begin() + batchIndex);
		std::vector<T>::iterator second(m_output.begin() + (batchIndex + next));

		std::copy(first, second, m_output.begin() + gathered);

		gathered += next;
	}

	m_output.resize(gathered);
}
template<class T>
inline void scatter_job<T>::finalize()
{
	std::vector<T>::reverse_iterator rIter(m_output.rbegin());
	std::uintptr_t gathered(0);

	for (std::size_t i = 0; i < m_batches; ++i, ++rIter){
		gathered += (std::uintptr_t)(*rIter);
	}

	m_output.resize(gathered);
}
template<class T>
inline void scatter_job<T>::prepare_output()
{
	m_output.resize(m_input.size() + m_chunks);
	std::uninitialized_fill(m_output.begin(), m_output.end(), nullptr);
}
template<class T>
inline void scatter_job<T>::process_batch(std::size_t begin, std::size_t end)
{
	std::uintptr_t done(0);

	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_input[i])){
			++done;
			m_output[begin + done] = m_input[i];
		}
	}

	const std::size_t batch(begin / m_batchSize);

	output_vector_type::reverse_iterator rIter(m_output.rend() + batch);

	*rIter = (T*)done;
}
}