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

#include <gdul/WIP_job_handler/job_interface.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/WIP_job_handler/scatter_job_impl.h>

namespace gdul {
namespace jh_detail {
template <class T>
class scatter_job_impl_abstr : public job_interface
{
public:
	scatter_job_impl_abstr(shared_ptr<scatter_job_impl<T>>&& job) 
		: m_impl(std::move(job)) 
	{}

	void add_dependency(job& dependency) {
		m_impl->add_dependency(dependency);
	}
	void set_priority(std::uint8_t priority) noexcept {
		m_impl->set_priority(priority);
	}
	void enable() {
		m_impl->enable();
	}
	bool is_finished() const noexcept {
		return m_impl->is_finished();
	}
	void wait_for_finish() noexcept {
		m_impl->wait_for_finish();
	}

private:
	shared_ptr<scatter_job_impl<T>> m_impl;
};
}
}