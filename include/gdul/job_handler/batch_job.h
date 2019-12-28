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

#include <gdul/job_handler/job_handler_commons.h>
#include <gdul/job_handler/job_interface.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul {
class job;

namespace jh_detail
{
template <class InputContainer, class OutputContainer, class Process>
class batch_job_impl;
}

class batch_job
{
public:
	batch_job();

	void add_dependency(job& dependency);
	void set_queue(std::uint8_t target) noexcept;
	void enable();
	bool is_finished() const noexcept;
	void wait_for_finish() noexcept;
	operator bool() const noexcept;
	job& get_endjob() noexcept;
	void set_name(const std::string& name);
private:
	friend class job_handler;

	template <class InputContainer, class OutputContainer, class Process>
	batch_job(shared_ptr<jh_detail::batch_job_impl<InputContainer, OutputContainer, Process>>&& job)
	: m_impl(std::move(job))
	{}

	shared_ptr<jh_detail::job_interface> m_impl;
};
}