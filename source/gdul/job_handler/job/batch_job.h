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
#include <gdul/job_handler/batch_job_impl_interface.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/job_handler/debug/job_tracker_interface.h>

namespace gdul {
class job;
class job_queue;

namespace jh_detail
{
template <class InContainer, class OutContainer, class Process>
class batch_job_impl;
}

class batch_job : public jh_detail::job_tracker_interface
{
public:
	batch_job();

	void add_dependency(job& dependency);

	// this object may be discarded once enable() has been invoked
	bool enable() noexcept;
	bool enable_locally_if_ready();

	bool is_finished() const noexcept;
	bool is_ready() const noexcept;

	void wait_until_finished() noexcept;
	void wait_until_ready() noexcept;

	// Consume jobs until finished. Beware of recursive calls (stack overflow, stalls etc..)
	void work_until_finished(job_queue* consumeFrom);

	// Consume jobs until ready. Beware of recursive calls (stack overflow, stalls etc..)
	void work_until_ready(job_queue* consumeFrom);

	operator bool() const noexcept;

	// Get the number of items written to the output container
	std::size_t get_output_size() const noexcept;
private:
	friend class job_handler;
	friend class job;

#if defined(GDUL_JOB_DEBUG)
	friend class jh_detail::job_tracker;
	constexpr_id register_tracking_node(constexpr_id id, const char* name, const char* file, std::uint32_t line, bool /*batchSub*/) override final;
#endif

	job& get_endjob() noexcept;

	template <class InContainer, class OutContainer, class Process>
	batch_job(shared_ptr<jh_detail::batch_job_impl<InContainer, OutContainer, Process>>&& job)
	: m_impl(std::move(job))
	{}

	shared_ptr<jh_detail::batch_job_impl_interface> m_impl;
};
}