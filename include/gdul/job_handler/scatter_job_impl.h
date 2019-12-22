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

#include <gdul/job_handler/job_handler_commons.h>
#include <gdul/job_handler/job.h>
#include <gdul/delegate/delegate.h>
#include <gdul/job_handler/job_interface.h>
// Get rid of this dependency.... belongs to implementation details. (does 'this' too for that matter)
#include <gdul/job_handler/chunk_allocator.h>

namespace gdul
{
class job_handler;

namespace jh_detail {

// Gets rid of circular dependency job_handler->scatter_job_impl & scatter_job_impl->job_handler
job _redirect_make_job(job_handler* handler, gdul::delegate<void()>&& workUnit);

template <class Container>
class scatter_job_impl : public job_interface
{
public:
	scatter_job_impl() = default;

	using container_type = Container;
	using value_type = typename Container::value_type;

	void add_dependency(job& dependency);

	void set_queue(std::uint8_t target) noexcept override final;

	void wait_for_finish() noexcept override final;

	void enable();
	bool is_finished() const noexcept override final;

	job& get_endjob() noexcept override final;

	scatter_job_impl(container_type& input, container_type& output, delegate<bool(value_type)>&& process, std::size_t batchSize, job_handler* handler);
	scatter_job_impl(container_type& inputOutput, delegate<bool(value_type)>&& process, std::size_t batchSize, job_handler* handler);
private:
	friend class gdul::job_handler;

	template <class Arr>
	std::size_t array_size(const Arr& arr) {
		return arr.end() - arr.begin();
	}

	std::size_t to_input_begin(std::size_t batchIndex) const;
	std::size_t to_input_end(std::size_t batchIndex) const;
	std::size_t to_output_begin(std::size_t batchIndex) const;
	std::size_t to_output_end(std::size_t batchIndex) const;

	bool is_input_output() const;

	void initialize();
	void finalize();

	void make_jobs();

	template <class Fun>
	job make_work_slice(Fun fun, std::size_t batchIndex);

	void work_process(std::size_t batchIndex);
	void work_pack(std::size_t batchIndex);

	std::size_t get_batch_pack_slot(std::size_t batchIndex);

	std::size_t base_batch_size() const {
		return m_batchSize;
	}
	// Accounting for the packing counter
	std::size_t offset_batch_size() const {
		return m_batchSize + 1;
	}

	job m_root;
	job m_end;

	delegate<bool(value_type)> m_process;

	container_type& m_input;
	container_type& m_output;

	const std::size_t m_batchSize;
	const std::size_t m_batchCount;
	const std::size_t m_baseArraySize;

	job_handler* const m_handler;

	std::uint8_t m_targetQueue;
};
template<class Container>
inline scatter_job_impl<Container>::scatter_job_impl(container_type& inputOutput, delegate<bool(value_type)>&& process, std::size_t batchSize, job_handler* handler)
	: scatter_job_impl<Container>::scatter_job_impl(inputOutput, inputOutput, std::move(process), batchSize, handler)
{}
template<class Container>
inline std::size_t scatter_job_impl<Container>::to_input_begin(std::size_t batchIndex) const
{
	return batchIndex * base_batch_size();
}
template<class Container>
inline std::size_t scatter_job_impl<Container>::to_input_end(std::size_t batchIndex) const
{
	const std::size_t desiredEnd(batchIndex * base_batch_size() + base_batch_size());
	if (!(m_baseArraySize < desiredEnd)) {
		return desiredEnd;
	}
	return m_baseArraySize;
}
template<class Container>
inline std::size_t scatter_job_impl<Container>::to_output_begin(std::size_t batchIndex) const
{
	if (!is_input_output()) {
		return batchIndex * offset_batch_size();
	}
	return to_input_begin(batchIndex);
}
template<class Container>
inline std::size_t scatter_job_impl<Container>::to_output_end(std::size_t batchIndex) const
{
	if (!is_input_output()) {
		const std::size_t desiredEnd(batchIndex * offset_batch_size() + offset_batch_size());
		if (!(array_size(m_output) < desiredEnd)) {
			return desiredEnd;
		}
		return array_size(m_output);
	}
	return to_input_end(batchIndex);
}
template<class Container>
inline bool scatter_job_impl<Container>::is_input_output() const
{
	return (void*)&m_input == (void*)&m_output;
}
template<class Container>
inline scatter_job_impl<Container>::scatter_job_impl(container_type& input, container_type& output, delegate<bool(value_type)>&& process, std::size_t batchSize, job_handler* handler)
	: m_root(_redirect_make_job(handler, delegate<void()>(&scatter_job_impl<Container>::initialize, this)))
	, m_end(_redirect_make_job(handler, delegate<void()>(&scatter_job_impl<Container>::finalize, this)))
	, m_process(std::move(process))
	, m_input(input)
	, m_output(output)
	, m_batchSize(batchSize + !(bool)batchSize)
	, m_batchCount(array_size(m_input) / base_batch_size() + ((bool)(array_size(m_input) % base_batch_size())))
	, m_baseArraySize(array_size(m_input))
	, m_handler(handler)
	, m_targetQueue(jh_detail::Default_Job_Queue)
{
	static_assert(std::is_pointer_v<value_type>, "value_type must be of pointer type");
}
template<class Container>
inline void scatter_job_impl<Container>::add_dependency(job& dependency)
{
	m_root.add_dependency(dependency);
}
template<class Container>
inline void scatter_job_impl<Container>::set_queue(std::uint8_t target) noexcept
{
	m_targetQueue = target;
}
template<class Container>
inline void scatter_job_impl<Container>::wait_for_finish() noexcept
{
	m_end.wait_for_finish();
}
template<class Container>
inline void scatter_job_impl<Container>::enable()
{
	m_root.enable();
}

template<class Container>
inline bool scatter_job_impl<Container>::is_finished() const noexcept
{
	return m_end.is_finished();
}

template<class Container>
inline job & scatter_job_impl<Container>::get_endjob() noexcept
{
	return m_end;
}

template<class Container>
inline void scatter_job_impl<Container>::initialize()
{
	m_output.resize(array_size(m_input) + m_batchCount);

	make_jobs();
}
template<class Container>
inline void scatter_job_impl<Container>::make_jobs()
{
	const std::size_t finalizeIndex(m_batchCount);

	job currentProcessJob = make_work_slice(&scatter_job_impl<Container>::work_process, 0);
	currentProcessJob.enable();

	job currentPackJob(currentProcessJob);

	for (std::size_t i = 1; i < m_batchCount; ++i) {
		job nextProcessJob(make_work_slice(&scatter_job_impl<Container>::work_process, i));

		if (i < (m_batchCount - 1)) {
			job nextPackJob(make_work_slice(&scatter_job_impl<Container>::work_pack, i));
			nextPackJob.add_dependency(currentPackJob);
			nextPackJob.add_dependency(nextProcessJob);
			nextPackJob.enable();
			
			currentPackJob = std::move(nextPackJob);
		}

		nextProcessJob.enable();
		currentProcessJob = std::move(nextProcessJob);
	}

	if (1 < m_batchCount){
		m_end.add_dependency(currentPackJob);
	}

	m_end.add_dependency(currentProcessJob);
	m_end.enable();
}
template<class Container>
template<class Fun>
inline job scatter_job_impl<Container>::make_work_slice(Fun fun, std::size_t batchIndex)
{
	job newJob(_redirect_make_job(m_handler, delegate<void()>(fun, this, batchIndex)));
	newJob.set_queue(m_targetQueue);

	return newJob;
}
template<class Container>
inline void scatter_job_impl<Container>::work_pack(std::size_t batchIndex)
{
	const std::size_t blah(array_size(m_input));
	blah;
	const std::size_t blahe(array_size(m_output));
	blahe;

	const std::size_t outputBegin(to_output_begin(batchIndex));
	const std::size_t outputEnd(to_output_end(batchIndex));

	assert(outputBegin != 0 && "Illegal to pack batch#0");
	assert(!(array_size(m_output) < outputEnd) && "End index out of bounds");

	const std::size_t packSlot(get_batch_pack_slot(batchIndex));

	auto batchEndStorageItr(m_output.begin() + packSlot);

	// Will never pack batch#0 
	std::uintptr_t lastBatchEnd((std::uintptr_t)*(m_output.begin() + outputBegin - 1));
	const std::uintptr_t batchSize((std::uintptr_t)(*batchEndStorageItr));

	auto copyBeginItr(m_output.begin() + outputBegin);
	auto copyEndItr(copyBeginItr + batchSize);
	auto copyTargetItr(m_output.begin() + lastBatchEnd);

	std::copy(copyBeginItr, copyEndItr, copyTargetItr);

	*batchEndStorageItr = (value_type)(lastBatchEnd + batchSize);
}
template<class Container>
inline void scatter_job_impl<Container>::work_process(std::size_t batchIndex)
{
	const std::size_t blah(array_size(m_input));
	blah;
	const std::size_t blahe(array_size(m_output));
	blahe;
	const std::size_t inputBegin(to_input_begin(batchIndex));
	const std::size_t inputEnd(to_input_end(batchIndex));
	const std::size_t outputBegin(to_output_begin(batchIndex));

	assert(!(array_size(m_output) < to_output_end(batchIndex)) && "End index out of bounds");

	const std::size_t packSlot(get_batch_pack_slot(batchIndex));
	std::uintptr_t batchOutputSize(0);

	for (std::size_t i = inputBegin; i < inputEnd; ++i) {
		if (m_process(*(m_input.begin() + i))) {
			*(m_output.begin() + outputBegin + batchOutputSize) = *(m_input.begin() + i);
			++batchOutputSize;
		}
	}

	*(m_output.begin() + packSlot) = (value_type)batchOutputSize;
}
template<class Container>
inline void scatter_job_impl<Container>::finalize()
{
	if (1 < m_batchCount) {
		work_pack(m_batchCount - 1);
	}

	const std::uintptr_t batchesEnd((std::uintptr_t)(m_output.back()));

	m_output.resize(batchesEnd);
}
template<class Container>
inline std::size_t scatter_job_impl<Container>::get_batch_pack_slot(std::size_t batchIndex)
{
	if (!is_input_output()) {
		const std::size_t last(array_size(m_output) - 1);
		const std::size_t desired(batchIndex * offset_batch_size() + (offset_batch_size() - 1));
		return desired < last ? desired : last;
	}
	else {
		return m_baseArraySize + (batchIndex);
	}
}
// Memory chunk representation of scatter_job_impl

struct dummy_container
{
	using value_type = void*;
};

struct alignas(alignof(std::max_align_t)) scatter_job_chunk_rep
{
	std::uint8_t dummy[alloc_size_make_shared<scatter_job_impl<dummy_container>, chunk_allocator<scatter_job_impl<dummy_container>, scatter_job_chunk_rep>>()]{};
};
}
}