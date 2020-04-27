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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/job_handler/job_handler_utility.h>
#include <gdul/job_handler/worker.h>
#include <gdul/job_handler/job.h>
#include <gdul/job_handler/batch_job_impl.h>
#include <gdul/job_handler/batch_job.h>
#include <gdul/delegate/delegate.h>
#include <gdul/pool_allocator/pool_allocator.h>

#pragma once

namespace gdul
{
namespace jh_detail
{
class job_handler_impl;
}

class job_handler
{
public:
	job_handler();
	job_handler(jh_detail::allocator_type allocator);
	~job_handler();

	void init();

	void retire_workers();

	std::size_t internal_worker_count() const noexcept;
	std::size_t external_worker_count() const noexcept;
	std::size_t active_job_count() const noexcept;

	static thread_local job this_job;
	static thread_local worker this_worker;

	worker make_worker();
	worker make_worker(delegate<void()> entryPoint);

	job make_job(delegate<void()> workUnit);

	// Invokes process for all elements
	// Requirements on Container is ::begin(), ::end(), ::value_type
	template <class InContainer>
	batch_job make_batch_job(
		InContainer& input,
		delegate<void(typename InContainer::value_type&)> process);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
	// Requirements on Container is ::begin(), ::end(), ::value_type, ::resize(std::size_t)
	template <class InOutContainer>
	batch_job make_batch_job(
		InOutContainer& inputOutput,
		delegate<bool(typename InOutContainer::value_type&)> process);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
	// Requirements on Container is ::begin(), ::end(), ::value_type, ::resize(std::size_t) (only output)
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(
		InContainer& input,
		OutContainer& output,
		delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process);

	// Invokes process for all elements
	// Requirements on Container is ::begin(), ::end(), ::value_type
	template <class InContainer>
	batch_job make_batch_job(
		InContainer& input,
		delegate<void(typename InContainer::value_type&)> process,
		std::size_t batchSize);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
	// Requirements on Container is ::begin(), ::end(), ::value_type
	template <class InOutContainer>
	batch_job make_batch_job(
		InOutContainer& inputOutput,
		delegate<bool(typename InOutContainer::value_type&)> process,
		std::size_t batchSize,
		delegate<void(std::size_t)> outputResizeFunc);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
	// Requirements on Container is ::begin(), ::end(), ::value_type. Input container size must not exceed that of output.
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(InContainer& input,
		OutContainer& output,
		delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process,
		std::size_t batchSize,
		delegate<void(std::size_t)> outputResizeFunc);
private:
	pool_allocator<jh_detail::dummy_batch_type> get_batch_job_allocator() const noexcept;

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;

	jh_detail::allocator_type m_allocator;
};
// Invokes process for all elements
// Requirements on Container is ::begin(), ::end(), ::value_type
template<class InContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	delegate<void(typename InContainer::value_type&)> process)
{
	const std::size_t containerSize((input.end() - input.begin()));
	const std::size_t approxBatchSize(containerSize / (internal_worker_count() * 3));
	return make_batch_job(input, std::move(process), approxBatchSize);
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
// Requirements on Container is ::begin(), ::end(), ::value_type, ::resize(std::size_t)
template<class InOutContainer>
inline batch_job job_handler::make_batch_job(
	InOutContainer& inputOutput, 
	delegate<bool(typename InOutContainer::value_type&)> process)
{
	const std::size_t containerSize((inputOutput.end() - inputOutput.begin()));
	const std::size_t approxBatchSize(containerSize / (internal_worker_count() * 3));
	return make_batch_job(inputOutput, std::move(process), approxBatchSize, [&inputOutput](std::size_t n) {inputOutput.resize(n); });
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
// Requirements on Container is ::begin(), ::end(), ::value_type, ::resize(std::size_t) (only output)
template<class InContainer, class OutContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	OutContainer& output, 
	delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process)
{
	const std::size_t containerSize((input.end() - input.begin()));
	const std::size_t approxBatchSize(containerSize / (internal_worker_count() * 3));
	return make_batch_job(input, output, std::move(process), approxBatchSize, [&output](std::size_t n) {output.resize(n); });
}
// Invokes process for all elements
// Requirements on Container is ::begin(), ::end(), ::value_type
template<class InContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	delegate<void(typename InContainer::value_type&)> process, 
	std::size_t batchSize)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, InContainer, delegate<void(typename InContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, input, input, std::move(process), batchSize, delegate<void(std::size_t)>([](std::size_t) {}), this);

	return batch_job(std::move(sp));
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
// Requirements on Container is ::begin(), ::end(), ::value_type
template<class InOutContainer>
inline batch_job job_handler::make_batch_job(
	InOutContainer& inputOutput, 
	delegate<bool(typename InOutContainer::value_type&)> process, 
	std::size_t batchSize, 
	delegate<void(std::size_t)> outputResizeFunc)
{
	using batch_type = jh_detail::batch_job_impl<InOutContainer, InOutContainer, delegate<bool(typename InOutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, inputOutput, inputOutput, std::move(process), batchSize, std::move(outputResizeFunc), this);

	return batch_job(std::move(sp));
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
// Requirements on Container is ::begin(), ::end(), ::value_type. Input container size must not exceed that of output.
template<class InContainer, class OutContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	OutContainer& output, 
	delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, 
	std::size_t batchSize, 
	delegate<void(std::size_t)> outputResizeFunc)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, OutContainer, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, input, output, std::move(process), batchSize, std::move(outputResizeFunc), this);

	return batch_job(std::move(sp));
}
}