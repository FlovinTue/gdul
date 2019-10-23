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

#include <cassert>
#include <gdul\WIP_job_handler\Job.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul
{
job::job()
	: myEnabled(false)
{
}
job::job(job && other)
{
	operator=(std::move(other));
}
job & job::operator=(job && other)
{
	myEnabled.store(other.myEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
	myImpl = std::move(other.myImpl);

	return *this;
}

void job::add_dependency(job & dependency)
{
	assert(myImpl && "Job not set");

	if (dependency.myImpl->try_attach_child(myImpl)) {
		myImpl->add_dependencies(1);
	}
}
void job::enable()
{
	if (myEnabled.exchange(false ,std::memory_order_relaxed)) {
		return;
	}

	assert(myImpl && "Job not set");

	if (myImpl->enable()) {
		myImpl->get_handler()->enqueue_job(myImpl);
	}
}
bool job::is_finished() const
{
	assert(myImpl && "Job not set");

	return myImpl->is_finished();
}
void job::wait_for_finish()
{
	assert(myImpl && "Job not set");

	while (!is_finished()) {
		job_handler::this_worker_impl->refresh_sleep_timer();
		job_handler::this_worker_impl->idle();
	}
}
job::operator bool() const
{
	return myImpl;
}
job::job(job_impl_shared_ptr impl)
	: myImpl(std::move(impl))
	, myEnabled{ATOMIC_FLAG_INIT}
{
}
}
