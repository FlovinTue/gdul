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
#include <gdul/WIP_job_handler/job_interface.h>

namespace gdul {

class job_handler;

namespace jh_detail {

class job_handler_impl;
class base_job_impl;

class base_job_abstr : public job_interface
{
public:
	base_job_abstr() noexcept;

	~base_job_abstr() noexcept;

	base_job_abstr(base_job_abstr&& other) noexcept;
	base_job_abstr(const base_job_abstr& other) noexcept;
	base_job_abstr& operator=(base_job_abstr&& other) noexcept;
	base_job_abstr& operator=(const base_job_abstr& other) noexcept;

	void add_dependency(job_interface& other) override;
	void set_priority(std::uint8_t priority) noexcept override;

	void enable();

	bool is_finished() const noexcept;

	void wait_for_finish() noexcept;

	operator bool() const noexcept;

private:
	friend class job_handler_impl;
	friend class job_handler;

	base_job_abstr(gdul::shared_ptr<base_job_impl> impl) noexcept;

	gdul::shared_ptr<base_job_impl> m_abstr;
};
}
}
