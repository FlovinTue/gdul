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
#include <../../Testers/Common/util.h>

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

	scatter_job(input_vector_type& input, output_vector_type& output, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);
	scatter_job(input_vector_type& inputOutput, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc);
private:
	friend class gdul::job_handler;

	std::size_t to_input_begin(std::size_t batchIndex) const;
	std::size_t to_input_end(std::size_t batchIndex) const;
	std::size_t to_output_begin(std::size_t batchIndex) const;
	std::size_t to_output_end(std::size_t batchIndex) const;

	bool is_input_output() const;

	void initialize();
	void finalize();

	void make_jobs();

	template <class U = value_type, std::enable_if_t<std::is_pointer_v<U>>* = nullptr>
	derefref_value_type copy_ref(ref_value_type ref){
		return ref;
	}

	template <class U = value_type, std::enable_if_t<!std::is_pointer_v<U>>* = nullptr>
	derefref_value_type copy_ref(ref_value_type ref){
		return &ref;
	}

	template <class Fun>
	job make_work_slice(Fun fun, std::size_t batchIndex);

	void work_process(std::size_t batchIndex);
	void work_pack(std::size_t batchIndex);

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

	delegate<bool(ref_value_type)> m_process;

	input_vector_type& m_input;
	output_vector_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	const std::size_t m_baseArraySize;
	
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
inline std::size_t scatter_job<T>::to_input_begin(std::size_t batchIndex) const
{
	return batchIndex * base_batch_size();
}
template<class T>
inline std::size_t scatter_job<T>::to_input_end(std::size_t batchIndex) const
{
	const std::size_t desiredEnd(batchIndex * base_batch_size() + base_batch_size());
	if (!(m_baseArraySize < desiredEnd)) {
		return desiredEnd;
	}
	return m_baseArraySize;
}
template<class T>
inline std::size_t scatter_job<T>::to_output_begin(std::size_t batchIndex) const
{
	if (!is_input_output()) {
		return batchIndex * offset_batch_size();
	}
	return to_input_begin(batchIndex);
}
template<class T>
inline std::size_t scatter_job<T>::to_output_end(std::size_t batchIndex) const
{
	if (!is_input_output()) {
		const std::size_t desiredEnd(batchIndex * offset_batch_size() + offset_batch_size());
		if (!(m_output.size() < desiredEnd)) {
			return desiredEnd;
		}
		return m_output.size();
	}
	return to_input_end(batchIndex);
}
template<class T>
inline bool scatter_job<T>::is_input_output() const
{
	return (void*)&m_input == (void*)&m_output;
}
template<class T>
inline scatter_job<T>::scatter_job(input_vector_type& input, output_vector_type& output, delegate<bool(ref_value_type)>&& process, std::size_t batchSize, job_handler* handler, jh_detail::allocator_type alloc)
	: m_root(handler->make_job(delegate<void()>(&scatter_job<T>::initialize, this)))
	, m_end(handler->make_job(delegate<void()>(&scatter_job<T>::finalize, this)))
	, m_process(std::move(process))
	, m_input(input)
	, m_output(output)
	, m_batchSize(batchSize + !(bool)batchSize)
	, m_batchCount(m_input.size() / base_batch_size() + ((bool)(m_input.size() % base_batch_size())))
	, m_baseArraySize(m_input.size())
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

		if (i < (m_batchCount - 1)) {
			job nextPackJob(make_work_slice(&scatter_job<T>::work_pack, i));
			nextPackJob.add_dependency(currentPackJob);
			nextPackJob.add_dependency(nextProcessJob);
			nextPackJob.enable();
			currentPackJob = std::move(nextPackJob);
		}

		currentProcessJob = std::move(nextProcessJob);
	}

	m_end.add_dependency(currentPackJob);
	m_end.enable();
}
template<class T>
template<class Fun>
inline job scatter_job<T>::make_work_slice(Fun fun, std::size_t batchIndex)
{
	job newJob(m_handler->make_job(delegate<void()>(fun, this, batchIndex)));
	newJob.set_priority(m_priority);

	return newJob;
}
template<class T>
inline void scatter_job<T>::work_pack(std::size_t batchIndex)
{
	const std::size_t outputBegin(to_output_begin(batchIndex));
	const std::size_t outputEnd(to_output_end(batchIndex));

	assert(outputBegin != 0 && "Illegal to pack batch#0");
	assert(!(m_output.size() < outputEnd) && "End index out of bounds");

	const std::size_t packSlot(get_batch_pack_slot(batchIndex));

	auto batchEndStorageItr(m_output.begin() + packSlot);

	// Will never pack batch#0 
	std::uintptr_t lastBatchEnd((std::uintptr_t)m_output[outputBegin - 1]);
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	auto copyBeginItr(m_output.begin() + outputBegin);
	auto copyEndItr(copyBeginItr + batchSize);
	auto copyTargetItr(m_output.begin() + lastBatchEnd);

	std::copy(copyBeginItr, copyEndItr, copyTargetItr);

	*batchEndStorageItr = (derefref_value_type)(lastBatchEnd + batchSize);
}
template<class T>
inline void scatter_job<T>::work_process(std::size_t batchIndex)
{
	const std::size_t inputBegin(to_input_begin(batchIndex));
	const std::size_t inputEnd(to_input_end(batchIndex));
	const std::size_t outputBegin(to_output_begin(batchIndex));

	assert(!(m_output.size() < to_output_end(batchIndex)) && "End index out of bounds");

	const std::size_t packSlot(get_batch_pack_slot(batchIndex));
	std::uintptr_t batchOutputSize(0);

	for (std::size_t i = inputBegin; i < inputEnd; ++i){
		if (m_process(m_input[i])){
			m_output[outputBegin + batchOutputSize] = copy_ref(m_input[i]);
			++batchOutputSize;
		}
	}

	m_output[packSlot] = (derefref_value_type)batchOutputSize;
}
template<class T>
inline void scatter_job<T>::finalize()
{
	if (1 < m_batchCount){
		work_pack(m_batchCount - 1);
	}

	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));
	m_output.resize(batchesEnd);
}
template<class T>
inline std::size_t scatter_job<T>::get_batch_pack_slot(std::size_t batchIndex)
{
	if (!is_input_output()) {
		const std::size_t last(m_output.size() - 1);
		const std::size_t desired(batchIndex * offset_batch_size() + (offset_batch_size() - 1));
		return desired < last ? desired : last;
	}
	else {
		return m_baseArraySize + (batchIndex);
	}
}
}