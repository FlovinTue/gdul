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
#include <gdul/job_handler/job/job.h>
#include <gdul/job_handler/tracking/job_graph.h>
#include <gdul/job_handler/job/batch_job_impl_interface.h>

#include <gdul/delegate/delegate.h>

#include <array>
#include <cassert>
#include <algorithm>
#include <cmath>

namespace gdul {
class job_handler;

namespace jh_detail {

struct dummy_batch_container { using value_type = int; };
using dummy_batch_type = batch_job_impl<dummy_batch_container, dummy_batch_container, delegate<bool(int&, int&)>>;

// Gets rid of circular dependency job_handler->batch_job_impl & batch_job_impl->job_handler
gdul::job _redirect_make_job(job_handler* handler, gdul::delegate<void()>&& workUnit, job_queue* target);

bool _redirect_enable_if_ready(gdul::shared_ptr<job_impl>& jb);
void _redirect_invoke_job(gdul::shared_ptr<job_impl>& jb);
bool _redirect_is_enabled(const gdul::shared_ptr<job_impl>& jb);
void _redirect_set_info(shared_ptr<job_handler_impl>& handler, gdul::shared_ptr<job_impl>& jb, std::size_t physicalId, std::size_t variationId, const char* name);

template <class InContainer, class OutContainer, class Process>
class batch_job_impl : public batch_job_impl_interface
{
public:
	batch_job_impl() = default;
	~batch_job_impl();

	using intput_container_type = InContainer;
	using output_container_type = OutContainer;
	using ref_input_type = typename intput_container_type::value_type&;
	using ref_output_type = typename output_container_type::value_type&;
	using process_type = Process;

	batch_job_impl(InContainer& input, OutContainer& output, Process&& process, std::size_t batchSize, gdul::delegate<void(std::size_t)> outputResizeFunc, job_handler* handler, job_queue* target);

	void depends_on(job& dependency) override final;

	void wait_until_finished() noexcept override final;
	void wait_until_ready() noexcept override final;
	void work_until_finished(job_queue* consumeFrom) override final;
	void work_until_ready(job_queue* consumeFrom) override final;

	bool enable(const shared_ptr<batch_job_impl_interface>& selfRef)  noexcept override final;
	bool enable_locally_if_ready() override final;

	bool is_enabled() const noexcept override final;
	bool is_finished() const noexcept override final;
	bool is_ready() const noexcept override final;

	job& get_endjob() noexcept override final;

	std::size_t get_output_size() const noexcept override final;

#if defined(GDUL_JOB_DEBUG)
	void track_sub_job(job& job, std::size_t variationId, const char* name);
#else
	void track_sub_job(job&, const char*) {};
#endif

private:
	friend class gdul::job_handler;

	template <class Cont>
	std::size_t container_size(const Cont& arr) const
	{
		return arr.size();
	}

	static constexpr bool Specialize_Input_Output = std::is_same_v<bool, typename process_type::return_type> && process_type::Num_Args == 2;
	static constexpr bool Specialize_Input = std::is_same_v<bool, typename process_type::return_type> && process_type::Num_Args == 1;
	static constexpr bool Specialize_Update = (!std::is_same_v<bool, typename process_type::return_type>) && process_type::Num_Args == 1;

	std::size_t to_batch_begin(std::size_t batchIndex) const;
	std::size_t to_batch_end(std::size_t batchIndex) const;

	std::uint32_t clamp_batch_size(std::size_t desired) const;

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

	GDUL_JOB_DEBUG_CONDTIONAL(job_info* m_info)
	GDUL_JOB_DEBUG_CONDTIONAL(timer m_completionTimer)
	GDUL_JOB_DEBUG_CONDTIONAL(timer m_enqueueTimer)

	std::array<std::uint32_t, Batch_Job_Max_Slices> m_batchTracker;

	process_type m_process;
	delegate<void(std::size_t)> m_outputResizeFunc;

	job_handler* const m_handler;
	job_queue* const m_target;

	bool (job::* m_enableFunc)(void);

	intput_container_type& m_input;
	output_container_type& m_output;

	const std::uint32_t m_batchSize;
	const std::uint16_t m_batchCount;

	atomic_shared_ptr<batch_job_impl_interface> m_selfRef;

	job m_root;
	job m_end;
};
template<class InContainer, class OutContainer, class Process>
inline batch_job_impl<InContainer, OutContainer, Process>::batch_job_impl(InContainer& input, OutContainer& output, Process&& process, std::size_t batchSize, gdul::delegate<void(std::size_t)> outputResizeFunc, job_handler* handler, job_queue* target)
	: m_batchTracker{}
	, m_process(std::move(process))
	, m_outputResizeFunc(std::move(outputResizeFunc))
	, m_handler(handler)
	, m_target(target)
	, m_enableFunc(&job::enable)
	, m_input(input)
	, m_output(output)
	, m_batchSize(clamp_batch_size(batchSize + !(bool)batchSize))
	, m_batchCount((std::uint16_t)(container_size(m_input) / m_batchSize + ((bool)(container_size(m_input) % m_batchSize))))
	, m_selfRef()
	, m_root(m_batchCount ? _redirect_make_job(handler, delegate<void()>(&batch_job_impl::initialize, this), target) : _redirect_make_job(m_handler, delegate<void()>([]() {}), target))
	, m_end(m_batchCount ? _redirect_make_job(handler, delegate<void()>(&batch_job_impl::finalize<>, this), target) : m_root)
#if defined(GDUL_JOB_DEBUG)
	, m_info(nullptr)
#endif
{}
template<class InContainer, class OutContainer, class Process>
inline batch_job_impl<InContainer, OutContainer, Process>::~batch_job_impl()
{
	assert(_redirect_is_enabled(m_root.m_impl) && "Job destructor ran before enable was called");
}
template<class InContainer, class OutContainer, class Process>
inline std::size_t batch_job_impl<InContainer, OutContainer, Process>::to_batch_begin(std::size_t batchIndex) const
{
	return batchIndex * m_batchSize;
}
template<class InContainer, class OutContainer, class Process>
inline std::size_t batch_job_impl<InContainer, OutContainer, Process>::to_batch_end(std::size_t batchIndex) const
{
	const std::size_t desiredEnd(batchIndex * m_batchSize + m_batchSize);
	if (!(container_size(m_input) < desiredEnd)) {
		return desiredEnd;
	}
	return (container_size(m_input));
}
template<class InContainer, class OutContainer, class Process>
inline std::uint32_t batch_job_impl<InContainer, OutContainer, Process>::clamp_batch_size(std::size_t desired) const
{
	const std::size_t resultantBatchCount(container_size(m_input) / desired + ((bool)(container_size(m_input) % desired)));
	const float resultF((float)resultantBatchCount);
	const float maxBatchCount((float)Batch_Job_Max_Slices);

	const float div(resultF / maxBatchCount);

	std::uint32_t returnValue((std::uint32_t)desired);

	if (1.f < div) {
		const float desiredF((float)desired);
		const float adjusted(desiredF * div);
		returnValue = (std::uint32_t)std::ceil(adjusted);
	}

	return returnValue;
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::depends_on(job& dependency)
{
	m_root.depends_on(dependency);
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::wait_until_finished() noexcept
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		m_end.wait_until_finished();
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::wait_until_ready() noexcept
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		m_root.wait_until_ready();
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_until_finished(job_queue* consumeFrom)
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		m_end.work_until_finished(consumeFrom);
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_until_ready(job_queue* consumeFrom)
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		m_root.work_until_ready(consumeFrom);
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
template<class InContainer, class OutContainer, class Process>
inline bool batch_job_impl<InContainer, OutContainer, Process>::enable(const shared_ptr<batch_job_impl_interface>& selfRef) noexcept
{
	GDUL_JOB_DEBUG_CONDTIONAL(m_enqueueTimer.reset())

		const bool result(m_root.enable());
	if (result) {
		raw_ptr<batch_job_impl_interface> expected(nullptr);
		m_selfRef.compare_exchange_strong(expected, selfRef, std::memory_order_relaxed);
	}

	return result;
}
template<class InContainer, class OutContainer, class Process>
inline bool batch_job_impl<InContainer, OutContainer, Process>::enable_locally_if_ready()
{
	if (_redirect_enable_if_ready(m_root.m_impl)) {

		GDUL_JOB_DEBUG_CONDTIONAL(m_enqueueTimer.reset())

			m_enableFunc = &job::enable_locally_if_ready;
		_redirect_invoke_job(m_root.m_impl);
		return true;
	}
	return false;
}
template<class InContainer, class OutContainer, class Process>
inline bool batch_job_impl<InContainer, OutContainer, Process>::is_enabled() const noexcept
{
	return _redirect_is_enabled(m_root.m_impl);
}
template<class InContainer, class OutContainer, class Process>
inline bool batch_job_impl<InContainer, OutContainer, Process>::is_finished() const noexcept
{
	return m_end.is_finished();
}
template<class InContainer, class OutContainer, class Process>
inline bool batch_job_impl<InContainer, OutContainer, Process>::is_ready() const noexcept
{
	return m_root.is_ready();
}
template<class InContainer, class OutContainer, class Process>
inline job& batch_job_impl<InContainer, OutContainer, Process>::get_endjob() noexcept
{
	return m_end;
}
template<class InContainer, class OutContainer, class Process>
inline std::size_t batch_job_impl<InContainer, OutContainer, Process>::get_output_size() const noexcept
{
	return m_batchTracker[m_batchCount - 1];
}
template<class InContainer, class OutContainer, class Process>
template<class Fun>
inline job batch_job_impl<InContainer, OutContainer, Process>::make_work_slice(Fun fun, std::size_t batchIndex)
{
	return _redirect_make_job(m_handler, delegate<void()>(fun, this, batchIndex), m_target);
}
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Input>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_process(std::size_t batchIndex)
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
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_process(std::size_t batchIndex)
{
	const std::size_t inputBegin(to_batch_begin(batchIndex));
	const std::size_t inputEnd(to_batch_end(batchIndex));

	for (std::size_t i = inputBegin; i < inputEnd; ++i) {
		ref_input_type inputRef(*(m_input.begin() + i));

		m_process(inputRef);
	}
}
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Input_Output>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_process(std::size_t batchIndex)
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
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<!U::Specialize_Update>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::make_jobs()
{
	job currentProcessJob = make_work_slice(&batch_job_impl::work_process<>, 0);
	track_sub_job(currentProcessJob, 1, "batch_job_process");

	std::invoke(m_enableFunc, &currentProcessJob);

	job currentPackJob(currentProcessJob);

	for (std::size_t i = 1; i < m_batchCount; ++i) {
		job nextProcessJob(make_work_slice(&batch_job_impl::work_process<>, i));
		track_sub_job(nextProcessJob, 2, "batch_job_process");
		std::invoke(m_enableFunc, &nextProcessJob);

		if (i < (m_batchCount - 1)) {
			job nextPackJob(make_work_slice(&batch_job_impl::work_pack, i));
			track_sub_job(nextPackJob, 3, "batch_job_pack");
			nextPackJob.depends_on(currentPackJob);
			nextPackJob.depends_on(nextProcessJob);
			std::invoke(m_enableFunc, &nextPackJob);

			currentPackJob = std::move(nextPackJob);
		}

		currentProcessJob = std::move(nextProcessJob);
	}

	if (1 < m_batchCount) {
		m_end.depends_on(currentPackJob);
	}

	track_sub_job(m_end, 4, "batch_job_finalize");

	m_end.depends_on(currentProcessJob);
	std::invoke(m_enableFunc, &m_end);
}
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::make_jobs()
{
	for (std::size_t i = 0; i < m_batchCount; ++i) {
		job processJob(make_work_slice(&batch_job_impl::work_process<>, i));
		track_sub_job(processJob, 5, "batch_job_process");
		std::invoke(m_enableFunc, &processJob);

		m_end.depends_on(processJob);
	}

	track_sub_job(m_end, 6, "batch_job_finalize");
	std::invoke(m_enableFunc, &m_end);
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::initialize()
{
	GDUL_JOB_DEBUG_CONDTIONAL(m_completionTimer.reset())
		GDUL_JOB_DEBUG_CONDTIONAL(if (m_info)m_info->m_enqueueTimeSet.log_time(m_enqueueTimer.get()))

		m_outputResizeFunc(container_size(m_input));

	assert(!(container_size(m_output) < container_size(m_input)) && "Input container size must not exceed output container size");

	make_jobs<>();
}
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::work_pack(std::size_t batchIndex)
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

	std::move(copyBeginItr, copyEndItr, copyTargetItr);

	m_batchTracker[batchIndex] = (std::uint32_t)(lastBatchEnd + batchSize);
}
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<!U::Specialize_Update>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::finalize()
{
	if (1 < m_batchCount) {
		work_pack(m_batchCount - 1);
	}

	m_outputResizeFunc(get_output_size());

	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info)m_info->m_completionTimeSet.log_time(m_completionTimer.get()))

		m_selfRef.store(shared_ptr<batch_job_impl_interface>(), std::memory_order_relaxed);
}
template<class InContainer, class OutContainer, class Process>
template <class U, std::enable_if_t<U::Specialize_Update>*>
inline void batch_job_impl<InContainer, OutContainer, Process>::finalize()
{
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info)m_info->m_completionTimeSet.log_time(m_completionTimer.get()))

		m_selfRef.store(shared_ptr<batch_job_impl_interface>(), std::memory_order_relaxed);
}
#if defined(GDUL_JOB_DEBUG)
template<class InContainer, class OutContainer, class Process>
inline void batch_job_impl<InContainer, OutContainer, Process>::track_sub_job(job& job, std::size_t variationId, const char* name)
{
	_redirect_set_info(m_handler->m_impl, job.m_impl, m_info->id(), variationId, name);
}
#endif
}
}