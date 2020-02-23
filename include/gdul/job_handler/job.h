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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/job_handler/job_handler_utility.h>
#include <gdul/job_handler/job_debug_interface.h>

namespace gdul {

class job_handler;
class batch_job;

namespace jh_detail {

class job_handler_impl;
class job_impl;
}
class job : public jh_detail::job_debug_interface
{
public:
	job() noexcept;

	~job();

	job(job&& other) noexcept;
	job(const job& other) noexcept;
	job& operator=(job&& other) noexcept;
	job& operator=(const job& other) noexcept;

	void add_dependency(job& dependency);
	void add_dependency(batch_job& dependency);

	void set_target_queue(job_queue target) noexcept;
	job_queue get_target_queue() const noexcept;

	// this object may be discarded once enable() has been invoked
	void enable();

	bool is_finished() const noexcept;

	void wait_until_finished() noexcept;

	// Consume jobs until finished. Beware of recursive calls (stack overflow, stalls etc..)
	void work_until_finished(job_queue consumeFrom);

	operator bool() const noexcept;

	// Get the duration of the job. Only valid if GDUL_JOB_DEBUG is defined & job has run
	float get_time() const noexcept;

private:
	friend class jh_detail::job_handler_impl;
	friend class jh_detail::job_debug_tracker;
	friend class job_handler;

#if defined(GDUL_JOB_DEBUG)
	void register_debug_node(const char* name, constexpr_id id) noexcept override final;
#endif

	job(gdul::shared_ptr<jh_detail::job_impl> impl) noexcept;

	gdul::shared_ptr<jh_detail::job_impl> m_impl;
};
}