// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_handler.h>
#include <gdul/WIP_job_handler/job.h>
#include <vector>



namespace gdul
{

// Desired usage scenario would be m_handler->make_scatter_job(...bunch of stuff)
// Which then returns .... job ? 
// Scatter job does not need to be enqueued. Since it only has subjobs
// If scatter_job inherits from job_impl or some hypothetical job_impl_base then we need to
// pay the vtable costs. Undesirable, since scatter_job will only (probably) represent a fraction of 
// the total consumed...


// Maybe abstract the (user)job class away with job->job_abstractor->[arbitrary impl]

template <class T>
class job_scatter
{
public:
	using value_type = T;
	using deref_value_type = std::remove_pointer_t<value_type>*;
	using input_vector_type = std::vector<value_type>;
	using output_vector_type = std::vector<deref_value_type>;


	// How to make job_scatter into a regular job?
	// We want (maybe) : 
	/*-------------------------------------------------------*/

	void add_dependency(job& dependency);
	
	void set_priority(std::uint8_t priority) noexcept;

	void wait_for_finish() noexcept;

	operator bool() const noexcept;

	void enable();
	bool is_finished() const;

private:
	job_scatter(input_vector_type& inputOutput, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);
	job_scatter(const input_vector_type& input, output_vector_type& output, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);

	void initialize();

	void make_jobs();

	job make_process_job(std::size_t batch);
	job make_pack_job();

	void process_batch(std::size_t begin, std::size_t end);
	void pack_batch(std::size_t begin, std::size_t end);
	void finalize_batches();

	job m_root;
	job m_end;

	const delegate<bool(deref_value_type)> m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	
	job_handler* const m_handler;

	jh_detail::allocator_type m_allocator;

	std::uint8_t m_priority;

	bool m_enabled;
};
template<class T>
inline void job_scatter<T>::add_dependency(job & dependency)
{
	m_root.add_dependency(dependency);
}
template<class T>
inline void job_scatter<T>::set_priority(std::uint8_t priority) noexcept
{
	m_priority = priority;
}
template<class T>
inline void job_scatter<T>::wait_for_finish() noexcept
{
	m_end.wait_for_finish();
}
template<class T>
inline job_scatter<T>::operator bool() const noexcept
{
	return true;
}
template<class T>
inline void job_scatter<T>::enable()
{
	m_root.enable();
}

template<class T>
inline bool job_scatter<T>::is_finished() const
{
	return m_end.is_finished();
}

template<class T>
inline job_scatter<T>::job_scatter(input_vector_type& inputOutput, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc)
	: job_scatter<T>::job_scatter(inputOutput, inputOutput, std::move(process), batchSize, handler, alloc)
{
	static_assert(std::is_pointer_v<value_type>, "value_type must be of pointer type when working with a single inputOutput array");
}
template<class T>
inline job_scatter<T>::job_scatter(const input_vector_type& input, output_vector_type& output, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc)
	: m_process(process)
	, m_input(input)
	, m_output(output)
	, m_batchSize(batchSize)
	, m_batchCount(m_input.size() / batchSize + ((bool)m_input.size() % batchSize))
	, m_handler(handler)
	, m_allocator(alloc)
	, m_priority(jh_detail::Default_Job_Priority)
{
}
template<class T>
inline void job_scatter<T>::make_jobs()
{
	job currentProcessJob(make_process_job());
	currentProcessJob.set_priority(m_priority);
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

	job finalizer(m_handler->make_job(&job_scatter<T>::finalize_batches, this));
	finalizer.add_dependency(currentPackJob);
	finalizer.enable();

	//*this->add_dependency(finalizer);
}
template<class T>
inline job job_scatter<T>::make_process_job(std::size_t batch)
{
	std::size_t begin(i * m_batchSize);
	std::size_t end(begin + m_batchSize < m_input.size() ? begin + m_batchSize : m_input.size());

	job newjob(job_handler::make_job(&job_scatter::process_batch, this, begin, end));

	return newJob;
}
template<class T>
inline job job_scatter<T>::make_pack_job()
{
	return job();
}
template<class T>
inline void job_scatter<T>::pack_batch(std::size_t begin, std::size_t end)
{
	assert(begin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < end) && "End going past vector limits");

	input_container_type::iterator batchEndStorageItr(m_output.at(end - 1));

	std::uintptr_t lastBatchEnd((std::uintptr_t)m_output[begin - 1]);
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	output_vector_type::iterator copyBegin(m_output.at(begin));
	output_vector_type::iterator copyEnd(first + batchSize);
	output_vector_type::iterator copyTarget(m_output.at(lastBatchEnd));

	std::copy(copyBegin, copyEnd, copyTarget);

	*batchEndStorage = (value_type*)(lastBatchEnd + batchSize);
}
template<class T>
inline void job_scatter<T>::finalize_batches()
{
	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));
	m_output.resize(batchesEnd);
}
template<class T>
inline void job_scatter<T>::initialize()
{
	m_output.resize(m_input.size() + m_batchCount);
}
template<class T>
inline void job_scatter<T>::process_batch(std::size_t begin, std::size_t end)
{
	std::uintptr_t batchOutputSize(0);

	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_input[i])){
			m_output[begin + batchOutputSize] = m_input[i];
			++batchOutputSize;
		}
	}

	m_output[end - 1] = (value_type*)batchOutputSize;
}
}