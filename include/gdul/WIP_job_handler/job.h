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

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_interface.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul {
namespace jh_detail
{
class base_job_abstr;

template <class T>
class scatter_job_impl_abstr;
template <class T>
class scatter_job_impl;
}

class job
{
public:
	job();
	~job();

	job(const job& other);
	job(job&& other);

	job& operator=(const job& other);
	job& operator=(job&& other);

	void add_dependency(job& other);
	void set_priority(std::uint8_t priority) noexcept;
	void enable();
	bool is_finished() const noexcept;
	void wait_for_finish() noexcept;
	operator bool() const noexcept;

private:
	jh_detail::base_job_abstr& get_depender();

	friend class job_handler;

	template <class JobAbstractor>
	job(JobAbstractor&& base_job_abstr) {
		m_abstr = new (&m_storage[0]) JobAbstractor(std::move(base_job_abstr));
	}

	struct job_abstr_rep : public jh_detail::job_interface{gdul::shared_ptr<int> dummy;};

	jh_detail::job_interface* m_abstr;
	std::uint8_t m_storage[sizeof(job_abstr_rep)];
};
}