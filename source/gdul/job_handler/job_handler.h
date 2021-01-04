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
#include <gdul/memory/pool_allocator.h>

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

	std::size_t worker_count() const noexcept;

	worker make_worker();

	/// <summary>
	/// Creates a basic job
	/// </summary>
	/// <param name="workUnit">Work to be executed</param>
	/// <param name="target">Scheduling target</param>
	/// <param name="id">Persistent identifier</param>
	/// <param name="dbgName">Job name</param>
	/// <returns>New job</returns>
	/// <summary>
	job make_job(delegate<void()> workUnit, job_queue* target, std::size_t id = 0, const char* dbgName = "") { workUnit; target; id; dbgName; /* See macro definition */}

	job _redirect_make_job(delegate<void()> workUnit, job_queue* target, std::size_t id, const char* dbgName, const char* dbgFile, std::size_t line, std::size_t extraId);
	job _redirect_make_job(delegate<void()> workUnit, job_queue* target, std::size_t id, const char* dbgFile, std::size_t line, std::size_t extraId);
	job _redirect_make_job(delegate<void()> workUnit, job_queue* target, const char* dbgFile, std::size_t line, std::size_t extraId);

	// Invokes process for all elements
	// Requirements on Container is ::size(), ::value_type, random access iterator
	// Will not modify container
	template <class InContainer>
	batch_job make_batch_job(
		InContainer& input,
		delegate<void(typename InContainer::value_type&)> process,
		std::size_t batchSize,
		job_queue* target);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
	// Requirements on Container is ::size(), ::value_type, random access iterator
	// Will modify container
	template <class InOutContainer>
	batch_job make_batch_job(
		InOutContainer& inputOutput,
		delegate<bool(typename InOutContainer::value_type&)> process,
		std::size_t batchSize,
		delegate<void(std::size_t)> outputResizeFunc,
		job_queue* target);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
	// Requirements on Container is ::size(), ::value_type, random access iterator. Input container size must not exceed that of output.
	// Will modify output container
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(InContainer& input,
		OutContainer& output,
		delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process,
		std::size_t batchSize,
		delegate<void(std::size_t)> outputResizeFunc,
		job_queue* target);

	// Invokes process for all elements
	// Requirements on Container is ::size(), ::value_type, random access iterator
	// Will not modify container
	template <class InContainer>
	batch_job make_batch_job(
		InContainer& input,
		delegate<void(typename InContainer::value_type&)> process,
		job_queue* target);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
	// Requirements on Container is ::size(), ::value_type, random access iterator, ::resize(std::size_t)
	// Will modify container
	template <class InOutContainer>
	batch_job make_batch_job(
		InOutContainer& inputOutput,
		delegate<bool(typename InOutContainer::value_type&)> process,
		job_queue* target);

	// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
	// Requirements on Container is ::size(), ::value_type, random access iterator, ::resize(std::size_t) (only output)
	// Will modify output container
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(
		InContainer& input,
		OutContainer& output,
		delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process,
		job_queue* target);
private:
	pool_allocator<jh_detail::dummy_batch_type> get_batch_job_allocator() const noexcept;

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;

	jh_detail::allocator_type m_allocator;
};
// Invokes process for all elements
// Requirements on Container is ::size(), ::value_type, random access iterator
// Will not modify container
template<class InContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	delegate<void(typename InContainer::value_type&)> process, 
	std::size_t batchSize,
	job_queue* target)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, InContainer, delegate<void(typename InContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, input, input, std::move(process), batchSize, delegate<void(std::size_t)>([](std::size_t) {}), this, target);

	return batch_job(std::move(sp));
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
// Requirements on Container is ::size(), ::value_type, random access iterator
// Will modify container
template<class InOutContainer>
inline batch_job job_handler::make_batch_job(
	InOutContainer& inputOutput, 
	delegate<bool(typename InOutContainer::value_type&)> process, 
	std::size_t batchSize, 
	delegate<void(std::size_t)> outputResizeFunc,
	job_queue* target)
{
	using batch_type = jh_detail::batch_job_impl<InOutContainer, InOutContainer, delegate<bool(typename InOutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, inputOutput, inputOutput, std::move(process), batchSize, std::move(outputResizeFunc), this, target);

	return batch_job(std::move(sp));
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
// Requirements on Container is ::size(), ::value_type, random access iterator. Input container size must not exceed that of output.
// Will modify output container
template<class InContainer, class OutContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input, 
	OutContainer& output, 
	delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, 
	std::size_t batchSize, 
	delegate<void(std::size_t)> outputResizeFunc,
	job_queue* target)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, OutContainer, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(alloc, input, output, std::move(process), batchSize, std::move(outputResizeFunc), this, target);

	return batch_job(std::move(sp));
}
// Invokes process for all elements
// Requirements on Container is ::size(), ::value_type, random access iterator
// Will not modify container
template<class InContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input,
	delegate<void(typename InContainer::value_type&)> process,
	job_queue* target)
{
	const std::size_t containerSize((input.end() - input.begin()));
	const std::size_t approxBatchSize(containerSize / (worker_count() * 3));
	return make_batch_job(input, std::move(process), approxBatchSize, target);
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the processed collection
// Requirements on Container is ::size(), ::value_type, random access iterator, ::resize(std::size_t)
// Will modify container
template<class InOutContainer>
inline batch_job job_handler::make_batch_job(
	InOutContainer& inputOutput,
	delegate<bool(typename InOutContainer::value_type&)> process,
	job_queue* target)
{
	const std::size_t containerSize((inputOutput.end() - inputOutput.begin()));
	const std::size_t approxBatchSize(containerSize / (worker_count() * 3));
	return make_batch_job(inputOutput, std::move(process), approxBatchSize, [&inputOutput](std::size_t n) {inputOutput.resize(n); }, target);
}
// Invokes process for all elements. Boolean returnvalue signals inclusion in the output collection
// Requirements on Container is ::size(), ::value_type, random access iterator, ::resize(std::size_t) (only output)
// Will modify output container
template<class InContainer, class OutContainer>
inline batch_job job_handler::make_batch_job(
	InContainer& input,
	OutContainer& output,
	delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process,
	job_queue* target)
{
	const std::size_t containerSize((input.end() - input.begin()));
	const std::size_t approxBatchSize(containerSize / (worker_count() * 3));
	return make_batch_job(input, output, std::move(process), approxBatchSize, [&output](std::size_t n) {output.resize(n); }, target);
}
}


#if  defined(_MSC_VER) || defined(__INTEL_COMPILER)
#if !defined (GDUL_INLINE_PRAGMA)
#define GDUL_INLINE_PRAGMA(pragma) __pragma(pragma)
#else
#define GDUL_STRINGIFY_PRAGMA(pragma) #pragma
#define GDUL_INLINE_PRAGMA(pragma) _Pragma(GDUL_STRINGIFY_PRAGMA(pragma))
#endif
#endif
// Expanding a bunch of physical properties of the declaration for later use (and debug use)
#define make_job(...) _redirect_make_job(__VA_ARGS__, __FILE__, __LINE__, \
GDUL_INLINE_PRAGMA(warning(push)) \
GDUL_INLINE_PRAGMA(warning(disable : 4307)) \
constexp_str_hash(__FILE__) \
GDUL_INLINE_PRAGMA(warning(pop)) \
+ std::size_t(__LINE__)
