// Copyright(c) 2020 Flovin Michaelsen
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

#include <gdul/job_handler/job_handler_utility.h>
#include <gdul/job_handler/job.h>
#include <gdul/delegate/delegate.h>
#include <gdul/job_handler/batch_job_impl_interface.h>
#include <gdul/job_handler/chunk_allocator.h>
#include <array>
#include <cassert>

namespace gdul
{
class job_handler;

namespace jh_detail {

// Gets rid of circular dependency job_handler->batch_job_impl & batch_job_impl->job_handler
job _redirect_make_job(job_handler* handler, gdul::delegate<void()>&& workUnit);

template <class InputContainer, class OutputContainer, class Process>
class batch_job_impl : public batch_job_impl_interface
{
public:
	batch_job_impl() = default;

	using intput_container_type = InputContainer;
	using output_container_type = OutputContainer;
	using ref_input_type = typename intput_container_type::value_type &;
	using ref_output_type = typename output_container_type::value_type &;
	using process_type = Process;

	void add_dependency(job& dependency);

	void set_target_queue(job_queue target) noexcept override final;
	job_queue get_target_queue() const noexcept override final;

	void wait_until_finished() noexcept override final;
	void work_until_finished(job_queue consumeFrom) override final;

	bool enable()  noexcept override final;
	bool is_finished() const noexcept override final;

	job& get_endjob() noexcept override final;

	std::size_t get_output_size() const noexcept override final;

#if defined(GDUL_JOB_DEBUG)
	constexpr_id register_tracking_node(constexpr_id id, const char* name, const char* file, std::uint32_t line) override final;
	void track_sub_job(job& job, const char* name);
#else
	void track_sub_job(job&, const char*) {};
#endif

	batch_job_impl(intput_container_type& input, output_container_type& output, process_type&& process, std::size_t batchSize, job_handler* handler);
	batch_job_impl(intput_container_type& inputOutput, process_type&& process, std::size_t batchSize, job_handler* handler);
private:
	friend class gdul::job_handler;

	template <class Cont>
	std::size_t container_size(const Cont& arr) const {
		return arr.end() - arr.begin();
	}

	static constexpr bool Specialize_Input_Output = std::is_same_v<bool, typename process_type::return_type> && Process::Num_Args == 2;
	static constexpr bool Specialize_Input = std::is_same_v<bool, typename process_type::return_type> && process_type::Num_Args == 1;
	static constexpr bool Specialize_Update = (!std::is_same_v<bool, typename process_type::return_type>) && process_type::Num_Args == 1; 

	std::size_t to_batch_begin(std::size_t batchIndex) const;
	std::size_t to_batch_end(std::size_t batchIndex) const;

	std::uint32_t clamp_batch_size(std::size_t desired) const;

	bool is_input_output() const;

	template <class Fun>
	job make_work_slice(Fun fun, std::size_t batchIndex);

	template <class U = batch_job_impl, std::enable_if_t<U::Specialize_Input_Output>* = nullptr>
	void work_process(std::size_t batchIndex);
	template <class U = batch_job_impl, std::enable_if_t<U::Specialize_Input>* = nullptr>
	void work_process(std::size_t batchIndex);
	template <class U = batch_job_impl, std::enable_if_t<U::Specialize_Update>* = nullptr>
	void work_process(std::size_t batchIndex);

	template <class U = batch_job_impl, std::enable_if_t<!U::Specialize_Update>* = nullptr>
	void make_jobs();
	template <class U = batch_job_impl, std::enable_if_t<U::Specialize_Update>* = nullptr>
	void make_jobs();

	void initialize();

	template <class U = batch_job_impl, std::enable_if_t<!U::Specialize_Update>* = nullptr>
	void finalize();
	template <class U = batch_job_impl, std::enable_if_t<U::Specialize_Update>* = nullptr>
	void finalize();

	void work_pack(std::size_t batchIndex);

#if defined(GDUL_JOB_DEBUG)
	job_tracker_node* m_trackingNode;
	timer m_timer;
#endif

	std::array<std::uint32_t, Batch_Job_Max_Batches> m_batchTracker;

	process_type m_process;

	job_handler* const m_handler;

	job m_root;
	job m_end;

	intput_container_type& m_input;
	output_container_type& m_output;

	const std::uint32_t m_batchSize;
	const std::uint16_t m_batchCount;

	job_queue m_targetQueue;
};
template<class InputContainer, class OutputContainer, class Process>
inline batch_job_impl<InputContainer, OutputContainer, Process>::batch_job_impl(InputContainer& inputOutput, typename batch_job_impl<InputContainer, OutputContainer, Process>::process_type&& process, std::size_t batchSize, job_handler* handler)
	: batch_job_impl<InputContainer, OutputContainer, Process>::batch_job_impl(inputOutput, inputOutput, std::move(process), batchSize, handler)
{}
template<class InputContainer, class OutputContainer, class Process>
inline batch_job_impl<InputContainer, OutputContainer, Process>::batch_job_impl(InputContainer& input, OutputContainer& output, typename batch_job_impl<InputContainer, OutputContainer, Process>::process_type&& process, std::size_t batchSize, job_handler* handler)
	: m_batchTracker{}
	, m_process(std::move(process))
	, m_handler(handler)
	, m_root(container_size(input) ? _redirect_make_job(handler, delegate<void()>(&batch_job_impl::initialize, this)) : _redirect_make_job(m_handler, delegate<void()>([]() {})))
	, m_end(container_size(input) ? _redirect_make_job(handler, delegate<void()>(&batch_job_impl::finalize<>, this)) : m_root)
	, m_input(input)
	, m_output(output)
	, m_batchSize(clamp_batch_size(batchSize + !(bool)batchSize))
	, m_batchCount((std::uint16_t)(container_size(m_input) / m_batchSize + ((bool)(container_size(m_input) % m_batchSize))) + !(bool)(container_size(m_input)))
	, m_targetQueue(jh_detail::Default_Job_Queue)
#if defined(GDUL_JOB_DEBUG)
	, m_trackingNode(nullptr)
#endif
{
	assert(!(container_size(m_input) < container_size(m_output)) && "Input container size must not exceed output container size");
}
template<class InputContainer, class OutputContainer, class Process>
inline std::size_t batch_job_impl<InputContainer, OutputContainer, Process>::to_batch_begin(std::size_t batchIndex) const
{
	return batchIndex * m_batchSize;
}
template<class InputContainer, class OutputContainer, class Process>
inline std::size_t batch_job_impl<InputContainer, OutputContainer, Process>::to_batch_end(std::size_t batchIndex) const
{
	const std::size_t desiredEnd(batchIndex * m_batchSize + m_batchSize);
	if (!(container_size(m_input) < desiredEnd)) {
		return desiredEnd;
	}
	return (container_size(m_input));
}
template<class InputContainer, class OutputContainer, class Process>
inline std::uint32_t batch_job_impl<InputContainer, OutputContainer, Process>::clamp_batch_size(std::size_t desired) const
{
	const std::size_t resultantBatchCount(container_size(m_input) / desired + ((bool)(container_size(m_input) % desired)));
	const float resultF((float)resultantBatchCount);
	const float maxBatchCount((float)Batch_Job_Max_Batches);

	const float div(resultF / maxBatchCount);

	std::uint32_t returnValue((std::uint32_t)desired);

	if (1.f < div) {
		const float desiredF((float)desired);
		const float adjusted(desiredF * div);
		returnValue = (std::uint32_t)std::ceil(adjusted);
	}

	return returnValue;
}
template<class InputContainer, class OutputContainer, class Process>
inline bool batch_job_impl<InputContainer, OutputContainer, Process>::is_input_output() const
{
	return (void*)&m_input == (void*)&m_output;
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::add_dependency(job& dependency)
{
	m_root.add_dependency(dependency);
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::set_target_queue(job_queue target) noexcept
{
	m_targetQueue = target;
}
template<class InputContainer, class OutputContainer, class Process>
inline job_queue batch_job_impl<InputContainer, OutputContainer, Process>::get_target_queue() const noexcept
{
	return m_targetQueue;
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::wait_until_finished() noexcept
{
	m_end.wait_until_finished();
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::work_until_finished(job_queue consumeFrom)
{
	m_end.work_until_finished(consumeFrom);
}
template<class InputContainer, class OutputContainer, class Process>
inline bool batch_job_impl<InputContainer, OutputContainer, Process>::enable() noexcept
{
	return m_root.enable();
}
template<class InputContainer, class OutputContainer, class Process>
inline bool batch_job_impl<InputContainer, OutputContainer, Process>::is_finished() const noexcept
{
	return m_end.is_finished();
}
template<class InputContainer, class OutputContainer, class Process>
inline job & batch_job_impl<InputContainer, OutputContainer, Process>::get_endjob() noexcept
{
	return m_end;
}
template<class InputContainer, class OutputContainer, class Process>
inline std::size_t batch_job_impl<InputContainer, OutputContainer, Process>::get_output_size() const noexcept
{
	return m_batchTracker[m_batchCount - 1];
}
template<class InputContainer, class OutputContainer, class Process>
template<class Fun>
inline job batch_job_impl<InputContainer, OutputContainer, Process>::make_work_slice(Fun fun, std::size_t batchIndex)
{
	job newJob(_redirect_make_job(m_handler, delegate<void()>(fun, this, batchIndex)));
	newJob.set_target_queue(m_targetQueue);

	return newJob;
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Input>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::work_process(std::size_t batchIndex)
{
	const std::size_t inputBegin(to_batch_begin(batchIndex));
	const std::size_t inputEnd(to_batch_end(batchIndex));

	std::size_t batchOutputSize(0);

	for (std::size_t i = inputBegin; i < inputEnd; ++i) {
		ref_input_type inputRef(*(m_input.begin() + i));

		if (m_process(inputRef)) {
			ref_input_type outputRef(*(m_input.begin() + batchOutputSize + inputBegin));

			if (&inputRef != &outputRef) {
				outputRef = inputRef;
			}

			++batchOutputSize;
		}
	}

	m_batchTracker[batchIndex] = (std::uint32_t)batchOutputSize;
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::work_process(std::size_t batchIndex)
{
	const std::size_t inputBegin(to_batch_begin(batchIndex));
	const std::size_t inputEnd(to_batch_end(batchIndex));

	for (std::size_t i = inputBegin; i < inputEnd; ++i) {
		ref_input_type inputRef(*(m_input.begin() + i));

		m_process(inputRef);
	}
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Input_Output>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::work_process(std::size_t batchIndex)
{
	const std::size_t inputBegin(to_batch_begin(batchIndex));
	const std::size_t inputEnd(to_batch_end(batchIndex));
	const std::size_t outputBegin(to_batch_begin(batchIndex));

	assert(!(container_size(m_output) < to_batch_end(batchIndex)) && "End index out of bounds");

	std::size_t batchOutputSize(0);

	for (std::size_t i = inputBegin; i < inputEnd; ++i) {
		ref_input_type inputRef(*(m_input.begin() + i));
		ref_output_type outputRef(*(m_output.begin() + outputBegin + batchOutputSize));

		if (m_process(inputRef, outputRef)) {
			++batchOutputSize;
		}
	}

	m_batchTracker[batchIndex] = (std::uint32_t)batchOutputSize;
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<!U::Specialize_Update>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::make_jobs()
{
	job currentProcessJob = make_work_slice(&batch_job_impl::work_process<>, 0);
	track_sub_job(currentProcessJob, "batch_job_process");

	currentProcessJob.enable();

	job currentPackJob(currentProcessJob);

	for (std::size_t i = 1; i < m_batchCount; ++i) {
		job nextProcessJob(make_work_slice(&batch_job_impl::work_process<>, i));
		track_sub_job(nextProcessJob, "batch_job_process");

		if (i < (m_batchCount - 1)) {
			job nextPackJob(make_work_slice(&batch_job_impl::work_pack, i));
			track_sub_job(nextPackJob, "batch_job_pack");
			nextPackJob.add_dependency(currentPackJob);
			nextPackJob.add_dependency(nextProcessJob);
			nextPackJob.enable();

			currentPackJob = std::move(nextPackJob);
		}

		nextProcessJob.enable();
		currentProcessJob = std::move(nextProcessJob);
	}

	if (1 < m_batchCount) {
		m_end.add_dependency(currentPackJob);
	}

	track_sub_job(m_end, "batch_job_finalize");

	m_end.add_dependency(currentProcessJob);
	m_end.enable();
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::make_jobs()
{
	for (std::size_t i = 0; i < m_batchCount; ++i) {
		job processJob(make_work_slice(&batch_job_impl::work_process<>, i));
		track_sub_job(processJob, "batch_job_process");

		m_end.add_dependency(processJob);
		processJob.enable();
	}

	track_sub_job(m_end, "batch_job_finalize");
	m_end.enable();
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::initialize()
{
#if defined(GDUL_JOB_DEBUG)
	m_timer.reset();
#endif
	make_jobs<>();
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::work_pack(std::size_t batchIndex)
{
	const std::size_t outputBegin(to_batch_begin(batchIndex));
	const std::size_t outputEnd(to_batch_end(batchIndex));

	assert(outputBegin != 0 && "Illegal to pack batch#0");
	assert(!(container_size(m_output) < outputEnd) && "End index out of bounds");

	// Will never pack batch#0 
	std::size_t lastBatchEnd(m_batchTracker[batchIndex - 1]);
	const std::size_t batchSize(m_batchTracker[batchIndex]);

	auto copyBeginItr(m_output.begin() + outputBegin);
	auto copyEndItr(copyBeginItr + batchSize);
	auto copyTargetItr(m_output.begin() + lastBatchEnd);

	std::copy(copyBeginItr, copyEndItr, copyTargetItr);

	m_batchTracker[batchIndex] = (std::uint32_t)(lastBatchEnd + batchSize);
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<!U::Specialize_Update>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::finalize()
{
	if (1 < m_batchCount) {
		work_pack(m_batchCount - 1);
	}

#if defined(GDUL_JOB_DEBUG)
	if (m_trackingNode)
		m_trackingNode->add_completion_time(m_timer.get());
#endif
}
template<class InputContainer, class OutputContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::finalize()
{
#if defined(GDUL_JOB_DEBUG)
	if (m_trackingNode)
		m_trackingNode->add_completion_time(m_timer.get());
#endif
}
#if defined(GDUL_JOB_DEBUG)
template<class InputContainer, class OutputContainer, class Process>
inline constexpr_id batch_job_impl<InputContainer, OutputContainer, Process>::register_tracking_node(constexpr_id id, const char* name, const char* file, std::uint32_t line)
{
	const constexpr_id batchJobNodeId(((job_tracker_interface*)(&m_root))->register_tracking_node(id, name, file, line));
	m_trackingNode = job_tracker::fetch_node(batchJobNodeId);
	m_trackingNode->set_node_type(job_tracker_node_batch);
	return batchJobNodeId;
}
template<class InputContainer, class OutputContainer, class Process>
inline void batch_job_impl<InputContainer, OutputContainer, Process>::track_sub_job(job & job, const char * name)
{
	if (m_trackingNode)
		((job_tracker_interface*)(&job))->register_tracking_node(m_trackingNode->id(), name, "", 0, true);
}
#endif
struct dummy_container{using value_type = int;};

// Memory chunk representation of batch_job_impl
#pragma warning(push)
#pragma warning(disable:4324)
struct alignas(alignof(std::max_align_t)) batch_job_chunk_rep
{
	std::uint8_t dummy[allocate_shared_size<batch_job_impl<dummy_container, dummy_container, gdul::delegate<bool(int&, int&)>>, chunk_allocator<batch_job_impl<dummy_container, dummy_container, gdul::delegate<bool(int&, int&)>>, batch_job_chunk_rep>>()]{};
};
#pragma warning(pop)
}
}