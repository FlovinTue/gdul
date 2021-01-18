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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/execution/job_handler/worker/worker.h>
#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/job_handler/job/batch_job_impl.h>
#include <gdul/execution/job_handler/job/batch_job.h>
#include <gdul/delegate/delegate.h>
#include <gdul/memory/pool_allocator.h>

#undef make_job
#undef make_batch_job

namespace gdul {
namespace jh_detail {
class job_handler_impl;
}

/// <summary>
/// A basic job system with support for job dependencies and parallelism-promoting scheduling. 
/// Intended for use in a frame based program
/// </summary>
class job_handler
{
public:

	/// <summary>
	/// Constructor
	/// </summary>
	job_handler();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="allocator">Allocator. Instance is propagated for use within job handler</param>
	job_handler(jh_detail::allocator_type allocator);

	/// <summary>
	/// Destructor
	/// </summary>
	~job_handler();

	/// <summary>
	/// Initialize
	/// </summary>
	void init();

	/// <summary>
	/// Destroy workers and de-initialize
	/// </summary>
	void shutdown();

	/// <summary>
	/// Query for workers
	/// </summary>
	/// <returns>Active workers</returns>
	std::size_t worker_count() const noexcept;

	/// <summary>
	/// Creates a worker
	/// </summary>
	/// <returns>New worker handle</returns>
	worker make_worker();

	/// <summary>
	/// Creates a basic job
	/// </summary>
	/// <param name="workUnit">Work to be executed</param>
	/// <param name="target">Scheduling target</param>
	/// <param name="dbgName">Job name</param>
	/// <returns>New job</returns>
	job make_job(delegate<void()> workUnit, job_queue* target, const char* dbgName = "") { workUnit; target; dbgName; /* See make_job macro definition */ }

	/// <summary>
	/// Creates a basic job
	/// </summary>
	/// <param name="workUnit">Work to be executed</param>
	/// <param name="target">Scheduling target</param>
	/// <param name="id">Persistent identifier. Used to keep track of this physical job instantiation</param>
	/// <param name="dbgName">Job name</param>
	/// <returns>New job</returns>
	job make_job(delegate<void()> workUnit, job_queue* target, std::size_t variationId, const char* dbgName = "") { workUnit; target; variationId; dbgName; /* See make_job macro definition */ }


	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Basically a parallel std::for_each utilizing jobs
	/// </summary>
	/// <typeparam name="InContainer">Input container. Requires size(), ::value_type and forward iterator</typeparam>
	/// <param name="input">Input container value</param>
	/// <param name="process">Processor called for each element</param>
	/// <param name="target">Target queue</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InContainer>
	batch_job make_batch_job(InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, const char* dbgName = "")
	{
		input; process; target; dbgName; /* See make_batch_job macro definition */
	}

	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Basically a parallel std::for_each utilizing jobs
	/// </summary>
	/// <typeparam name="InContainer">Input container. Requires size(), ::value_type and forward iterator</typeparam>
	/// <param name="input">Input container value</param>
	/// <param name="process">Processor called for each element</param>
	/// <param name="target">Target queue</param>
	/// <param name="variationId">Persistent identifier. Used to keep track of this physical job instantiation</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InContainer>
	batch_job make_batch_job(InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "")
	{
		input; process; target; variationId; dbgName; /* See make_batch_job macro definition */
	}

	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Reduces the input container based on processor returnvalue
	/// </summary>
	/// <typeparam name="InOutContainer">Container type. Will be reduced to the elements that passes the processing step. Requires size(), resize(), ::value_type and forward iterator</typeparam>
	/// <param name="inputOutput">Input container value</param>
	/// <param name="process">Processor called for each element. Returnvalue determines if element is to be included in the final state of the container</param>
	/// <param name="target">Target queue</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InOutContainer>
	batch_job make_batch_job(InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, const char* dbgName = "")
	{
		inputOutput; process; target; dbgName; /* See make_batch_job macro definition */
	}

	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Reduces the input container based on processor returnvalue
	/// </summary>
	/// <typeparam name="InOutContainer">Container type. Will be reduced to the elements that passes the processing step. Requires size(), resize(), ::value_type and forward iterator</typeparam>
	/// <param name="inputOutput">Input container value</param>
	/// <param name="process">Processor called for each element. Returnvalue determines if element is to be included in the final state of the container</param>
	/// <param name="target">Target queue</param>
	/// <param name="variationId">Persistent identifier. Used to keep track of this physical job instantiation</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InOutContainer>
	batch_job make_batch_job(InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "")
	{
		inputOutput; process; target; variationId; dbgName; /* See make_batch_job macro definition */
	};

	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Outputs the (potentially reduced) set of input items to a separate output container
	/// </summary>
	/// <typeparam name="InContainer">Container to be read from. Requires size(), ::value_type and forward iterator</typeparam>
	/// <typeparam name="OutContainer">Container to be written to. Requires size(), resize(), ::value_type and forward iterator</typeparam>
	/// <param name="input">Input container value</param>
	/// <param name="output">Output container value</param>
	/// <param name="process">Processor called for each element. Returnvalue determines if element is to be included in the output container</param>
	/// <param name="target">Target queue</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, const char* dbgName = "")
	{
		input; output; process; target; dbgName; /* See make_batch_job macro definition */
	}

	/// <summary>
	/// Creates a batch job for splitting up processing of container elements. Outputs the (potentially reduced) set of input items to a separate output container
	/// </summary>
	/// <typeparam name="InContainer">Container to be read from. Requires size(), ::value_type and forward iterator</typeparam>
	/// <typeparam name="OutContainer">Container to be written to. Requires size(), resize(), ::value_type and forward iterator</typeparam>
	/// <param name="input">Input container value</param>
	/// <param name="output">Output container value</param>
	/// <param name="process">Processor called for each element. Returnvalue determines if element is to be included in the output container</param>
	/// <param name="target">Target queue</param>
	/// <param name="variationId">Persistent identifier. Used to keep track of this physical job instantiation</param>
	/// <param name="dbgName">Job debug name</param>
	/// <returns>New batch job</returns>
	template <class InContainer, class OutContainer>
	batch_job make_batch_job(InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "")

	{
		input; output; process; target; variationId; dbgName; /* See make_batch_job macro definition */
	}

#if defined (GDUL_JOB_DEBUG)
	/// <summary>
	/// Write the current job graph to a dgml file
	/// </summary>
	void dump_job_graph();

	/// <summary>
	/// Write the current job graph to a dgml file
	/// </summary>
	void dump_job_time_sets();

	/// <summary>
	/// Write the current job graph to a dgml file
	/// </summary>
	/// <param name="location">Output file location</param>
	void dump_job_graph(const char* location);

	/// <summary>
	/// Write job timing sets to file
	/// </summary>
	/// <param name="location">Output file location</param>
	void dump_job_time_sets(const char* location);
#endif

	//// Not for direct use
	//job _redirect_make_job(delegate<void()> workUnit, job_queue* target, std::size_t variationId, const char* dbgName, std::size_t physicalId, const char* dbgFile, std::uint32_t line);
	//// Not for direct use
	//job _redirect_make_job(delegate<void()> workUnit, job_queue* target, std::size_t variationId, std::size_t physicalId, const char* dbgFile, std::uint32_t line);
	//// Not for direct use
	//job _redirect_make_job(delegate<void()> workUnit, job_queue* target, const char* dbgName, std::size_t physicalId, const char* dbgFile, std::uint32_t line);
	//// Not for direct use
	//job _redirect_make_job(delegate<void()> workUnit, job_queue* target, std::size_t physicalId, const char* dbgFile, std::uint32_t line);


	// Not for direct use
	job _redirect_make_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, delegate<void()> workUnit, job_queue* target, std::size_t variationId, const char* dbgName = "");
	// Not for direct use
	job _redirect_make_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, delegate<void()> workUnit, job_queue* target, const char* dbgName = "");


	// Not for direct use
	template <class InContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, const char* dbgName = "");
	template <class InContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "");
	template <class InOutContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, const char* dbgName = "");
	template <class InOutContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "");
	template <class InContainer, class OutContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, const char* dbgName = "");
	template <class InContainer, class OutContainer>
	batch_job _redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, std::size_t variationId, const char* dbgName = "");

private:
	template <class InContainer, class OutContainer, class Process>
	friend class jh_detail::batch_job_impl;

	pool_allocator<jh_detail::dummy_batch_type> get_batch_job_allocator() const noexcept;
	jh_detail::job_info* get_job_info(std::size_t physicalId, std::size_t variationId, const char* dbgName, const char* dbgFile, std::uint32_t line);

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;

	jh_detail::allocator_type m_allocator;
};

template<class InContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, const char* dbgName)
{
	return _redirect_make_batch_job(physicalId, dbgFile, line, input, std::move(process), target, 0, dbgName);
}

template<class InContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, std::size_t variationId, [[maybe_unused]] const char* dbgName)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, InContainer, delegate<void(typename InContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(
		alloc,
		input,
		input,
		std::move(process),
		get_job_info(physicalId, variationId, dbgName, dbgFile, line),
		m_impl.get(),
		target);

	return batch_job(std::move(sp));
}

template<class InOutContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, const char* dbgName)
{
	return _redirect_make_batch_job(physicalId, dbgFile, line, inputOutput, std::move(process), target, 0, dbgName);
}
template<class InOutContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, std::size_t variationId, [[maybe_unused]] const char* dbgName)
{
	using batch_type = jh_detail::batch_job_impl<InOutContainer, InOutContainer, delegate<bool(typename InOutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(
		alloc,
		inputOutput,
		inputOutput,
		std::move(process),
		get_job_info(physicalId, variationId, dbgName, dbgFile, line),
		m_impl.get(),
		target);

	return batch_job(std::move(sp));
}

template<class InContainer, class OutContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, const char* dbgName)
{
	return _redirect_make_batch_job(physicalId, dbgFile, line, input, output, std::move(process), target, 0, dbgName);
}
template<class InContainer, class OutContainer>
inline batch_job job_handler::_redirect_make_batch_job(std::size_t physicalId, const char* dbgFile, std::uint32_t line, InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, std::size_t variationId, [[maybe_unused]] const char* dbgName)
{
	using batch_type = jh_detail::batch_job_impl<InContainer, OutContainer, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)>>;

	pool_allocator<batch_type> alloc(get_batch_job_allocator());

	shared_ptr<batch_type> sp = gdul::allocate_shared<batch_type>(
		alloc,
		input,
		output,
		std::move(process),
		get_job_info(physicalId, variationId, dbgName, dbgFile, line),
		m_impl.get(),
		target);

	return batch_job(std::move(sp));
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

// Signature 1:  gdul::job (delegate<void()> workUnit, job_queue* target, (opt) const char* dbgName)
// Signature 2:  gdul::job (delegate<void()> workUnit, job_queue* target, std::size_t variationId, (opt) const char* dbgName)
#define make_job(...) _redirect_make_job( \
GDUL_INLINE_PRAGMA(warning(push)) \
GDUL_INLINE_PRAGMA(warning(disable : 4307)) \
gdul::jh_detail::constexp_str_hash(__FILE__) \
GDUL_INLINE_PRAGMA(warning(pop)) \
* std::size_t(__LINE__) \
, __FILE__, __LINE__, __VA_ARGS__)


// Signature 1:  gdul::batch_job (InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, (opt) const char* dbgName)
// Signature 2:  gdul::batch_job (InContainer& input, delegate<void(typename InContainer::value_type&)> process, job_queue* target, std::size_t variationId, (opt) const char* dbgName)
// Signature 3:  gdul::batch_job (InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, (opt) const char* dbgName)
// Signature 4:  gdul::batch_job (InOutContainer& inputOutput, delegate<bool(typename InOutContainer::value_type&)> process, job_queue* target, std::size_t variationId, (opt) const char* dbgName)
// Signature 5:  gdul::batch_job (InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, (opt) const char* dbgName)
// Signature 6:  gdul::batch_job (InContainer& input, OutContainer& output, delegate<bool(typename InContainer::value_type&, typename OutContainer::value_type&)> process, job_queue* target, std::size_t variationId, (opt) const char* dbgName)
#define make_batch_job(...) _redirect_make_batch_job( \
GDUL_INLINE_PRAGMA(warning(push)) \
GDUL_INLINE_PRAGMA(warning(disable : 4307)) \
gdul::jh_detail::constexp_str_hash(__FILE__) \
GDUL_INLINE_PRAGMA(warning(pop)) \
* std::size_t(__LINE__) \
, __FILE__, __LINE__, __VA_ARGS__)