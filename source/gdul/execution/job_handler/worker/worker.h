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

#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/utility/delegate.h>
#include <string>
namespace gdul
{
template <class Signature>
class delegate;
class job_queue;
class thread;

namespace jh_detail
{
class worker_impl;
class job_impl;
}
class worker
{
public:
	static thread_local worker this_worker;

	worker() = default;
	worker(jh_detail::worker_impl* impl);
	worker(const worker&) = default;
	worker(worker&&) = default;
	worker& operator=(const worker&) = default;
	worker& operator=(worker&&) = default;

	~worker();

	/// <summary>
	/// Add queue from which to consume work. May be called multiple times
	/// </summary>
	/// <param name="queue">Source queue</param>
	void add_assignment(job_queue* queue);

	const thread* get_thread() const;
	thread* get_thread();

	void enable();
	bool disable();

	void set_run_on_enable(delegate<void()> toCall);
	void set_run_on_disable(delegate<void()> toCall);

	bool is_active() const;

private:
	friend class jh_detail::job_impl;

	jh_detail::worker_impl* m_impl;
};
}