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
template <class T, class Allocator = jh_detail::allocator_type>
class scatter_job
{
public:
	using value_type = T;
	using deref_value_type = std::remove_pointer_t<value_type>*;
	using input_vector_type = std::vector<value_type>;
	using output_vector_type = std::vector<deref_value_type>;

	scatter_job(input_vector_type& inputOutput, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, Allocator alloc = Allocator());
	scatter_job(const input_vector_type& input, output_vector_type& output, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, Allocator alloc = Allocator());

private:
	void initialize();

	void make_jobs();

	job make_process_job();
	job make_pack_job();

	void process_batch(std::size_t begin, std::size_t end);
	void pack_batch(std::size_t begin, std::size_t end);
	void finalize_batches();

	const delegate<bool(deref_value_type)> m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	
	job_handler* const m_handler;

	Allocator m_allocator;
};

template<class T, class Allocator>
inline scatter_job<T, Allocator>::scatter_job(input_vector_type& inputOutput, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, Allocator alloc)
	: scatter_job<T, Allocator>::scatter_job(inputOutput, inputOutput, std::move(process), batchSize, handler, alloc)
{
	static_assert(std::is_pointer_v<value_type>, "value_type must be of pointer type when working with a single inputOutput array");
}
template<class T, class Allocator>
inline scatter_job<T, Allocator>::scatter_job(const input_vector_type& input, output_vector_type& output, delegate<bool(deref_value_type)> process, std::size_t batchSize, job_handler* handler, Allocator alloc)
	: m_process(process)
	, m_input(input)
	, m_output(output)
	, m_batchSize(batchSize)
	, m_batchCount(m_input.size() / batchSize + ((bool)m_input.size() % batchSize))
	, m_handler(handler)
	, m_allocator(alloc)
{
}
template<class T, class Allocator>
inline void scatter_job<T, Allocator>::make_jobs()
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

	job finalizer(m_handler->make_job(&scatter_job<T, Allocator>::finalize_batches, this));
	finalizer.add_dependency(currentPackJob);
	finalizer.enable();

	//*this->add_dependency(finalizer);
}
template<class T, class Allocator>
inline job scatter_job<T, Allocator>::make_process_job()
{
	std::size_t begin(i * m_batchSize);
	std::size_t end(begin + m_batchSize < m_input.size() ? begin + m_batchSize : m_input.size());

	job newjob(job_handler::make_job(&scatter_job::process_batch, this, begin, end));

	return newJob;
}
template<class T, class Allocator>
inline job scatter_job<T, Allocator>::make_pack_job()
{
	return job();
}
template<class T, class Allocator>
inline void scatter_job<T, Allocator>::pack_batch(std::size_t begin, std::size_t end)
{
	assert(begin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < end) && "End going past vector limits");

	input_container_type::iterator batchEndStorageItr(m_output.at(end - 1));

	std::uintptr_t lastBatchEnd((std::uintptr_t)m_output[begin - 1]);
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	Container::iterator copyBegin(m_output.at(begin));
	Container::iterator copyEnd(first + batchSize);
	Container::iterator copyTarget(m_output.at(lastBatchEnd));

	std::copy(copyBegin, copyEnd, copyTarget);

	*batchEndStorage = (value_type*)(lastBatchEnd + batchSize);
}
template<class T, class Allocator>
inline void scatter_job<T, Allocator>::finalize_batches()
{
	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));
	m_output.resize(batchesEnd);
}
template<class T, class Allocator>
inline void scatter_job<T, Allocator>::initialize()
{
	m_input.resize(m_input.size() + m_batchCount);
}
template<class T, class Allocator>
inline void scatter_job<T, Allocator>::process_batch(std::size_t begin, std::size_t end)
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