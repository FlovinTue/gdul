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
#include <gdul/job_handler/job_handler_utility.h>

namespace gdul {

class job_handler;
class batch_job;
class job_queue;

namespace jh_detail {

class job_handler_impl;
class job_impl;
struct job_info;
class job_graph;
class worker_impl;

template <class InContainer, class OutContainer, class Process>
class batch_job_impl;
}
class job
{
public:
	static thread_local job this_job;

	job() noexcept;

	job(job&& other) noexcept;
	job(const job& other) noexcept;
	job& operator=(job&& other) noexcept;
	job& operator=(const job& other) noexcept;

	void add_dependency(job& dependency);
	void add_dependency(batch_job& dependency);

	// this object may be discarded once enable() has been invoked
	bool enable() noexcept;
	bool enable_locally_if_ready() noexcept;

	bool is_ready() const noexcept;
	bool is_finished() const noexcept;

	void wait_until_finished() noexcept;
	void wait_until_ready() noexcept;

	// Consume jobs until finished. Beware of recursive calls (stack overflow, stalls etc..)
	void work_until_finished(job_queue* consumeFrom);

	// Consume jobs until ready. Beware of recursive calls (stack overflow, stalls etc..)
	void work_until_ready(job_queue* consumeFrom);

	operator bool() const noexcept;

	float priority() const noexcept;

private:
	friend class job_handler;
	friend class jh_detail::worker_impl;
	template <class InContainer, class OutContainer, class Process>
	friend class jh_detail::batch_job_impl;
	friend class jh_detail::job_handler_impl;
	friend class jh_detail::job_graph;

#if defined(GDUL_JOB_DEBUG)
	friend class jh_detail::job_graph;

	job_info* get_job_info(std::size_t id, const char* name, const char* file, std::uint32_t line, bool batchSub) override final;
#endif

	job(gdul::shared_ptr<jh_detail::job_impl> impl) noexcept;

	gdul::shared_ptr<jh_detail::job_impl> m_impl;
};
}