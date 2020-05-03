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
#include <gdul/delegate/delegate.h>
#include <string>

namespace gdul
{
template <class Signature>
class delegate;

namespace jh_detail
{
class worker_impl;
}
class worker
{
public:
	worker() = default;
	worker(jh_detail::worker_impl* impl);
	worker(const worker&) = default;
	worker(worker&&) = default;
	worker& operator=(const worker&) = default;
	worker& operator=(worker&&) = default;

	~worker();

	void set_core_affinity(std::uint8_t core);
	void set_execution_priority(std::int32_t priority);
	void set_name(const std::string& name);

	void enable();
	bool disable();

	void set_run_on_enable(delegate<void()> toCall);
	void set_run_on_disable(delegate<void()> toCall);

	void set_queue_consume_first(job_queue firstQueue) noexcept;
	void set_queue_consume_last(job_queue lastQueue) noexcept;

	bool is_active() const;

private:
	friend class job_handler_impl;

	jh_detail::worker_impl* m_impl;
};
}