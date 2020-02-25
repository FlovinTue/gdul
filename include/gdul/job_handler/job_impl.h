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


#include <gdul/job_handler/job_handler_utility.h>
#include <gdul/job_handler/chunk_allocator.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/job_handler/job_node.h>
#include <gdul/delegate/delegate.h>

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/job_debug_tracker.h>
#endif

namespace gdul{

namespace jh_detail {

class job_handler_impl;

class job_impl
{
public:
	using allocator_type = gdul::jh_detail::allocator_type; 

	using job_impl_shared_ptr = shared_ptr<job_impl>;
	using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl>;
	using job_impl_raw_ptr = raw_ptr<job_impl>;

	job_impl();

	job_impl(delegate<void()>&& workUnit, job_handler_impl* handler);
	~job_impl();
	
	void operator()();

	bool try_attach_child(job_impl_shared_ptr child);

	job_queue get_target_queue() const noexcept;
	void set_target_queue(job_queue target) noexcept;

	bool try_add_dependencies(std::uint32_t n = 1);
	std::uint32_t remove_dependencies(std::uint32_t n = 1);

	bool enable();

	job_handler_impl* get_handler() const;

	bool is_finished() const;
	bool is_enabled() const;

#if defined(GDUL_JOB_DEBUG)
	void register_debug_node(const char* name, constexpr_id id) noexcept;
#endif

	float get_time() const noexcept;

	void work_until_finished(job_queue consumeFrom);
	void wait_until_finished() noexcept;
private:

	void detach_children();

#if defined(GDUL_JOB_DEBUG)
	std::string m_name;
	constexpr_id m_debugId;
	float m_time;
#endif

	delegate<void()> m_workUnit;

	job_handler_impl* const m_handler;

	atomic_shared_ptr<job_node> m_firstDependee;

	std::atomic<std::uint32_t> m_dependencies;

	std::atomic_bool m_finished;
	std::atomic_bool m_enabled;

	job_queue m_targetQueue;
};
// Memory chunk representation of job_impl
struct alignas(alignof(job_impl)) job_impl_chunk_rep
{
	std::uint8_t dummy[allocate_shared_size<job_impl, chunk_allocator<jh_detail::job_impl, job_impl_chunk_rep>>()]{};
};
}
}

#pragma warning(pop)

