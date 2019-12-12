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
template <class T>
class scatter_job
{
public:
	scatter_job() = default;

	using value_type = T;
	using derefref_value_type = std::remove_pointer_t<value_type>*;
	using ref_value_type = T & ;
	using input_vector_type = std::vector<value_type>;
	using output_vector_type = std::vector<derefref_value_type>;

	void add_dependency(job& dependency);
	
	void set_priority(std::uint8_t priority) noexcept;

	void wait_for_finish() noexcept;

	operator bool() const noexcept;

	void enable();
	bool is_finished() const;

private:
	scatter_job(input_vector_type& inputOutput, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);
	scatter_job(const input_vector_type& input, output_vector_type& output, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);

	void initialize();
	void finalize();

	void make_jobs();

	template <class WorkFun>
	job make_work_slice(WorkFun fun, std::size_t batchIndex);

	void work_process(std::size_t begin, std::size_t end);
	void work_pack(std::size_t begin, std::size_t end);

	std::size_t get_batch_pack_slot(std::size_t batchIndex);

	std::size_t base_batch_size() const{
		return m_batchSize;
	}
	// Accounting for the packing counter
	std::size_t offset_batch_size() const{
		return m_batchSize + 1;
	}

	job m_root;
	job m_end;

	const delegate<bool(ref_value_type)> m_process;

	const input_vector_type& m_input;
	output_vector_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	
	job_handler* const m_handler;

	jh_detail::allocator_type m_allocator;

	std::uint8_t m_priority;
};
template<class T>
inline scatter_job<T>::scatter_job(input_vector_type& inputOutput, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc)
	: scatter_job<T>::scatter_job(inputOutput, inputOutput, std::move(process), batchSize, handler, alloc)
{
	static_assert(std::is_pointer_v<value_type>, "value_type must be of pointer type when working with a single inputOutput array");
}
template<class T>
inline scatter_job<T>::scatter_job(const input_vector_type& input, output_vector_type& output, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc)
	: m_root(handler->make_job(delegate<void()>(&scatter_job<T>::initialize, this)))
	, m_end(handler->make_job(delegate<void()>&scatter_job<T>::finalize, this))
	, m_process(std::move(process))
	, m_input(input)
	, m_output(output)
	, m_batchSize(batchSize + !(bool)batchSize)
	, m_batchCount(m_input.size() / base_batch_size() + ((bool)m_input.size() % base_batch_size()))
	, m_handler(handler)
	, m_allocator(alloc)
	, m_priority(jh_detail::Default_Job_Priority)
{
}
template<class T>
inline void scatter_job<T>::add_dependency(job & dependency)
{
	m_root.add_dependency(dependency);
}
template<class T>
inline void scatter_job<T>::set_priority(std::uint8_t priority) noexcept
{
	m_priority = priority;
}
template<class T>
inline void scatter_job<T>::wait_for_finish() noexcept
{
	m_end.wait_for_finish();
}
template<class T>
inline scatter_job<T>::operator bool() const noexcept
{
	return true;
}
template<class T>
inline void scatter_job<T>::enable()
{
	m_root.enable();
}

template<class T>
inline bool scatter_job<T>::is_finished() const
{
	return m_end.is_finished();
}

template<class T>
inline void scatter_job<T>::initialize()
{
	m_output.resize(m_input.size() + m_batchCount);

	make_jobs();
}
template<class T>
inline void scatter_job<T>::make_jobs()
{
	const std::size_t finalizeIndex(m_batchCount);

	job currentProcessJob = make_work_slice(&scatter_job<T>::work_process, 0);
	currentProcessJob.enable();

	job currentPackJob(currentProcessJob);

	for(std::size_t i = 1; i < m_batchCount; ++i){
		job nextProcessJob(make_work_slice(&scatter_job<T>::work_process, i));
		nextProcessJob.enable();

		job nextPackJob(make_work_slice(&scatter_job<T>::work_pack, i));
		nextPackJob.add_dependency(currentPackJob);
		nextPackJob.add_dependency(nextProcessJob);
		nextPackJob.enable();

		currentProcessJob = std::move(nextProcessJob);
		currentPackJob = std::move(nextPackJob);
	}

	m_end.add_dependency(currentPackJob);
	m_end.enable();
}
template<class T>
template<class WorkFun>
inline job scatter_job<T>::make_work_slice(WorkFun fun, std::size_t batchIndex)
{
	const std::size_t begin(batchIndex * offset_batch_size());
	const std::size_t desiredEnd(begin + offset_batch_size());
	const std::size_t end(desiredEnd < m_input.size() ? desiredEnd : m_input.size() - 1);

	job newJob(job_handler::make_job(delegate<void()>(fun, this, begin, end)));
	newJob.set_priority(m_priority);
	
	return newJob;
}
template<class T>
inline void scatter_job<T>::work_pack(std::size_t begin, std::size_t)
{
	assert(begin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < end) && "End index out of bounds");

	const std::size_t packSlot(get_batch_pack_slot(begin / offset_batch_size()));

	typename output_vector_type::iterator batchEndStorageItr(m_output.at(packSlot));

	// Will never pack batch#0 
	std::uintptr_t lastBatchEnd((std::uintptr_t)m_output[begin - 1]);
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	typename output_vector_type::iterator copyBegin(m_output.at(begin));
	typename output_vector_type::iterator copyEnd(copyBegin + batchSize);
	typename output_vector_type::iterator copyTarget(m_output.at(lastBatchEnd));

	std::copy(copyBegin, copyEnd, copyTarget);

	*batchEndStorageItr = (value_type*)(lastBatchEnd + batchSize);
}
template<class T>
inline void scatter_job<T>::work_process(std::size_t begin, std::size_t end)
{
	assert(begin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < end) && "End index out of bounds");

	std::uintptr_t batchOutputSize(0);

	for (std::uint32_t i = begin; i < end; ++i){
		if (m_process(m_input[i])){
			m_output[begin + batchOutputSize] = m_input[i];
			++batchOutputSize;
		}
	}

	m_output[get_batch_pack_slot(begin / offset_batch_size())] = (value_type*)batchOutputSize;
}
template<class T>
inline void scatter_job<T>::finalize()
{
	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));
	m_output.resize(batchesEnd);
}
template<class T>
inline std::size_t scatter_job<T>::get_batch_pack_slot(std::size_t batchIndex)
{
	if (m_input != m_output) {
		return offset_batch_size() * (batchIndex + 1);
	}
	else {
		m_input.size() + (batchIndex);
	}
}
}