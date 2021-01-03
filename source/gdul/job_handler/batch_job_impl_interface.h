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

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/debug/job_tracker.h>
#endif

namespace gdul {
template <class T>
class shared_ptr;
class job;
namespace jh_detail {
class batch_job_impl_interface
{
public:
	virtual void add_dependency(job&) = 0;
	virtual bool enable(const shared_ptr<batch_job_impl_interface>&) noexcept = 0;
	virtual bool enable_locally_if_ready() = 0;
	virtual bool is_finished() const noexcept = 0;
	virtual bool is_ready() const noexcept = 0;
	virtual bool is_enabled() const noexcept = 0;
	virtual void wait_until_finished() noexcept = 0;
	virtual void work_until_finished(job_queue) = 0;
	virtual void wait_until_ready() noexcept = 0;
	virtual void work_until_ready(job_queue) = 0;

	virtual job& get_endjob() noexcept = 0;
	virtual std::size_t get_output_size() const noexcept = 0;
#if defined(GDUL_JOB_DEBUG)
	virtual constexpr_id register_tracking_node(constexpr_id, const char* name, const char* file, std::uint32_t line)  = 0;
#endif
};
}
}