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





// Hmm. Maybe not make this a 'job' type object. As lifetime needs to persist.
template <class T>
class scatter_job
{
public:
	using value_type = T;
	using deref_value_type = std::remove_pointer_t<T>;
	using input_vector_type = std::vector<value_type>;
	using output_vector_type = std::vector<deref_value_type*>;

	scatter_job(const input_vector_type& dataSource, output_vector_type& dataTarget, std::size_t batchSize, job_delegate process);

	

private:

	void initialize();

	void make_jobs();

	job make_process_job();
	job make_pack_job();

	void process_batch(std::size_t begin, std::size_t end);
	void pack_batch(std::size_t begin, std::size_t end);
	void finalize_batches();

	const job_delegate m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	
	job_handler* const m_handler;
};

template<class T>
inline scatter_job<T>::scatter_job(const input_vector_type& dataSource, output_vector_type& dataTarget, std::size_t batchSize, job_delegate process)
	: m_process(process)
	, m_input(dataSource)
	, m_output(dataTarget)
	, m_batchSize(batchSize)
	, m_batchCount(m_input.size() / batchSize + ((bool)m_input.size() % batchSize))
{
}
template<class T>
inline void scatter_job<T>::make_jobs()
{
	job currentProcessJob(make_process_job());
	currentProcessJob.enable();

	job currentPackJob(currentProcessJob);

	for(std::size_t i = 1; i < m_batchCount; ++i){
		job nextProcessJob(make_process_job());
		nextProcessJob.enable();

		job nextPackJob(make_pack_job());
		nextPackJob.add_dependency(currentPackJob);
		nextPackJob.add_dependency(nextProcessJob);
		nextPackJob.enable();

		currentProcessJob = std::move(nextProcessJob);
		currentPackJob = std::move(nextPackJob);
	}

	job finalizer(m_handler->make_job(&scatter_job<T>::finalize_batches, this));
	finalizer.add_dependency(currentPackJob);
	finalizer.enable();

	//*this->add_dependency(finalizer);
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
inline job scatter_job<T>::make_pack_job()
{
	return job();
}
template<class T>
inline void scatter_job<T>::pack_batch(std::size_t begin, std::size_t end)
{
	assert(begin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < end) && "End going past vector limits");

	std::vector<T>::iterator batchEndStorageItr(m_output.at(end - 1));

	std::uintptr_t lastBatchEnd((std::uintptr_t)m_output[begin - 1]);
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	std::vector<T>::iterator copyBegin(m_output.at(begin));
	std::vector<T>::iterator copyEnd(first + batchSize);
	std::vector<T>::iterator copyTarget(m_output.at(lastBatchEnd));

	std::copy(copyBegin, copyEnd, copyTarget);

	*batchEndStorage = (T*)(lastBatchEnd + batchSize);
}
template<class T>
inline void scatter_job<T>::finalize_batches()
{
	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));
	m_output.resize(batchesEnd);
}
template<class T>
inline void scatter_job<T>::initialize()
{
	m_output.resize(m_input.size() + m_chunks);
	std::uninitialized_fill(m_output.begin(), m_output.end(), nullptr);
}
template<class T>
inline void scatter_job<T>::process_batch(std::size_t begin, std::size_t end)
{
	std::uintptr_t batchOutputSize(0);

	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_input[i])){
			m_output[begin + batchOutputSize] = m_input[i];
			++batchOutputSize;
		}
	}

	m_output[end - 1] = (T*)batchOutputSize;
}
}