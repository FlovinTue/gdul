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
#pragma warning(push)
#pragma warning(disable : 4324)

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/chunk_allocator.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/WIP_job_handler/job_dependee.h>
#include <gdul/WIP_job_handler/job_delegate.h>

namespace gdul{

namespace jh_detail {

class job_handler_impl;
class job_delegate_base;

class job_impl
{
public:
	using allocator_type = gdul::jh_detail::allocator_type; 

	using job_impl_shared_ptr = shared_ptr<job_impl>;
	using job_impl_atomic_shared_ptr = atomic_shared_ptr<job_impl>;
	using job_impl_raw_ptr = raw_ptr<job_impl>;

	job_impl();

	job_impl(const job_delegate & call, job_handler_impl* handler);
	~job_impl();
	
	void operator()();

	bool try_attach_child(job_impl_shared_ptr child);

	std::uint8_t get_priority() const;
	void set_priority(std::uint8_t priority);

	void add_dependencies(std::uint32_t n = 1);
	std::uint32_t remove_dependencies(std::uint32_t n = 1);

	bool enable();

	job_handler_impl* get_handler() const;

	bool is_finished() const;
private:

	void detach_children();

	job_delegate m_callable;

	job_handler_impl* const m_handler;

	atomic_shared_ptr<job_dependee> m_firstDependee;

	std::atomic<std::uint32_t> m_dependencies;

	std::atomic_bool m_finished;
	std::atomic_bool m_enabled;

	std::uint8_t m_priority;
};

constexpr size_t blah = sizeof(job_impl);
// Memory chunk representation of job_impl
struct alignas(alignof(job_impl)) job_impl_chunk_rep
{
	job_impl_chunk_rep() : dummy{} {}
	uint8_t dummy[alloc_size_make_shared<job_impl, chunk_allocator<jh_detail::job_impl, job_impl_chunk_rep>>()];
};
}
}

#pragma warning(pop)

