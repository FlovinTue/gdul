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
#pragma warning(push)
#pragma warning(disable : 4324)

#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/execution/job_handler/tracking/job_graph.h>
#include <gdul/execution/job_handler/tracking/timer.h>
#include <gdul/execution/job_handler/job/job_node.h>

#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/utility/delegate.h>

namespace gdul {
class job_queue;

namespace jh_detail {

struct job_node;
struct job_info;

enum enable_result : std::uint8_t
{
	enable_result_null = 0,
	enable_result_enabled = 1 << 0,
	enable_result_enqueue = 1 << 1,
};

class job_handler_impl;

class job_impl
{
public:
	using allocator_type = gdul::jh_detail::allocator_type;

	using job_impl_shared_ptr = shared_ptr<job_impl>;
	using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl>;
	using job_impl_raw_ptr = raw_ptr<job_impl>;

	job_impl();
	job_impl(delegate<void()>&& workUnit, job_handler_impl* handler, job_queue* target, job_info* info);

	~job_impl();

	void operator()();

	bool try_attach_child(job_impl_shared_ptr child);

	bool try_add_dependencies(std::uint32_t n = 1);
	std::uint32_t remove_dependencies(std::uint32_t n = 1);

	enable_result enable() noexcept;
	bool enable_if_ready() noexcept;

	job_queue* get_target() const noexcept;

	bool is_finished() const noexcept;
	bool is_enabled() const noexcept;
	bool is_ready() const noexcept;

	void work_until_finished(job_queue* consumeFrom);
	void work_until_ready(job_queue* consumeFrom);
	void wait_until_finished() noexcept;
	void wait_until_ready() noexcept;

	float get_remaining_dependant_time() const noexcept;
	float get_remaining_propagation_time() const noexcept;

	std::size_t get_id() const noexcept;

	void set_info(job_info* info);

#if defined GDUL_JOB_DEBUG
	void on_enqueue() noexcept;
#endif

private:
	void detach_children();
	static void detach_next(job_node_shared_ptr from);

	delegate<void()> m_workUnit;

	job_info* m_info;

	timer m_completionTimer;

#if defined GDUL_JOB_DEBUG
	timer m_enqueueTimer;
#endif

	job_handler_impl* const m_handler;
	job_queue* const m_target;

	atomic_shared_ptr<job_node> m_headDependee;

	std::atomic<std::uint32_t> m_dependencies;

	std::atomic_bool m_finished;
};
}
}

#pragma warning(pop)

