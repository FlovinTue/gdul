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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/job_handler/job_handler_commons.h>
#include <gdul/job_handler/worker.h>
#include <gdul/job_handler/job.h>
#include <gdul/job_handler/batch_job_impl.h>
#include <gdul/job_handler/batch_job.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <gdul/job_handler/chunk_allocator.h>
#include <gdul/delegate/delegate.h>

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

	static thread_local job this_job;
	static thread_local worker this_worker;

	template <class InputContainer>
	batch_job make_batch_job(
		InputContainer& input,
		gdul::delegate<void(typename InputContainer::value_type&)>&& process,
		std::size_t batchSize);

	// Requirements on Container is begin() / end() iterators as well as resize() and Container::value_type definition
	// The process returnvalue signals inclusion/exclusion of an item in the output container. Expected process signature is
	// bool(inputItem&)
	template <class InputOutputContainer>
	batch_job make_batch_job(
		InputOutputContainer& inputOutput,
		gdul::delegate<bool(typename InputOutputContainer::value_type&)>&& process,
		std::size_t batchSize);

	// Requirements on Container is begin() / end() iterators as well as resize() and Container::value_type definition
	// The process returnvalue signals inclusion/exclusion of an item in the output container. Expected process signature is
	// bool(inputItem&, outputItem&)
	template <class InputContainer, class OutputContainer>
	batch_job make_batch_job(
		InputContainer& input,
		OutputContainer& output,
		gdul::delegate<bool(typename InputContainer::value_type&, typename OutputContainer::value_type&)>&& process,
		std::size_t batchSize);

	worker make_worker();
	worker make_worker(const worker_info& info);

	job make_job(gdul::delegate<void()>&& workUnit);

	void retire_workers();

	std::size_t num_workers() const;
	std::size_t num_enqueued() const;
private:
	concurrent_object_pool<jh_detail::batch_job_chunk_rep, jh_detail::allocator_type>* get_batch_job_chunk_pool();

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;

	jh_detail::allocator_type m_allocator;
};

template<class InputContainer>
inline batch_job job_handler::make_batch_job(
	InputContainer& input,
	gdul::delegate<void(typename InputContainer::value_type&)>&& process,
	std::size_t batchSize)
{
	using batch_type = jh_detail::batch_job_impl<InputContainer, InputContainer, gdul::delegate<void(typename InputContainer::value_type&)>>;

	jh_detail::chunk_allocator<batch_type, jh_detail::batch_job_chunk_rep> alloc(get_batch_job_chunk_pool());

	shared_ptr<batch_type> sp;
	sp = make_shared<batch_type, decltype(alloc)>(alloc, input, std::forward<decltype(process)>(process), batchSize, this);
	return batch_job(std::move(sp));
}

// Requirements on Container is begin() / end() iterators as well as resize() and Container::value_type definition
// The process returnvalue signals inclusion/exclusion of an item in the output container. Expected process signature is
// bool(inputItem&, outputItem&)
template<class InputOutputContainer>
inline batch_job job_handler::make_batch_job(
	InputOutputContainer& inputOutput,
	gdul::delegate<bool(typename InputOutputContainer::value_type&)>&& process,
	std::size_t batchSize)
{
	using batch_type = jh_detail::batch_job_impl<InputOutputContainer, InputOutputContainer, gdul::delegate<bool(typename InputOutputContainer::value_type&)>>;

	jh_detail::chunk_allocator<batch_type, jh_detail::batch_job_chunk_rep> alloc(get_batch_job_chunk_pool());

	shared_ptr<batch_type> sp;
	sp = make_shared<batch_type, decltype(alloc)>(alloc, inputOutput, std::forward<decltype(process)>(process), batchSize, this);
	return batch_job(std::move(sp));
}
// Requirements on Container is begin() / end() iterators as well as resize() and Container::value_type definition
// The process returnvalue signals inclusion/exclusion of an item in the output container. Expected process signature is
// bool(inputItem&, outputItem&)
template<class InputContainer, class OutputContainer>
inline batch_job job_handler::make_batch_job(
	InputContainer& input,
	OutputContainer& output,
	gdul::delegate<bool(typename InputContainer::value_type&, typename OutputContainer::value_type&)>&& process,
	std::size_t batchSize)
{
	using batch_type = jh_detail::batch_job_impl<InputContainer, OutputContainer, gdul::delegate<bool(typename InputContainer::value_type&, typename OutputContainer::value_type&)>>;

	jh_detail::chunk_allocator<batch_type, jh_detail::batch_job_chunk_rep> alloc(get_batch_job_chunk_pool());

	shared_ptr<batch_type> sp;
	sp = make_shared<batch_type, decltype(alloc)>(alloc, input, output, std::forward<decltype(process)>(process), batchSize, this);
	return batch_job(std::move(sp));
}
}