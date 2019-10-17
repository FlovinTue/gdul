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

#include <gdul\WIP_job_handler\Job.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul
{
job::job(job && other)
{
	operator=(std::move(other));
}
job & job::operator=(job && other)
{
	myImpl = std::move(other.myImpl);

	return *this;
}
void job::add_dependency(job & dependency)
{
	if (dependency.myImpl->try_attach_child(myImpl)) {
		myImpl->add_dependency();
	}
}
void job::enable()
{
	if (myEnabled.test_and_set(std::memory_order_relaxed)) {
		return;
	}

	myImpl->enable();
}
job::job(job_impl_shared_ptr impl)
	: myImpl(std::move(impl))
	, myEnabled{ATOMIC_FLAG_INIT}
{
}
}
