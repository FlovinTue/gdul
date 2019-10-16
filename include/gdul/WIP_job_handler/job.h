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

#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {

class job_handler;

namespace job_handler_detail {

class job_impl;
}
class job
{
public:
	template <class Callable>
	job(job_handler* handler, Callable&& callable);
	template <class Callable>
	job(job_handler* handler, Callable&& callable, std::uint8_t priority);
	template <class Callable>
	job(job_handler* handler, Callable&& callable, job& dependency);
	template <class Callable>
	job(job_handler* handler, Callable&& callable, job& dependency, std::uint8_t priority);

	~job() noexcept;

	job(job&& other);
	job& operator=(job&& other);

	job(const job&) = delete;
	job& operator=(const job&) = delete;

private:
	job_handler_detail::job_impl_shared_ptr myImpl;
};

template<class Callable>
inline job::job(job_handler * handler, Callable && callable)
	: job(handler, std::forward<Callable&&>(callable), job_handler_detail::Default_Job_Priority)
{
}

template<class Callable>
inline job::job(job_handler * handler, Callable && callable, std::uint8_t priority)
{
	handler->enqueue_job(handler->make_job(handler, std::forward<Callable&&>(callable), priority);
}

template<class Callable>
inline job::job(job_handler * handler, Callable && callable, job & dependency)
	: job(handler, std::forward<Callable&&>(callable), dependency, job_handler_detail::Default_Job_Priority)
{
}

template<class Callable>
inline job::job(job_handler * handler, Callable && callable, job & dependency, std::uint8_t priority)
{
	myImpl = handler->make_job(handler, std::forward<Callable&&>(callable), priority);

	// OR something. 
	if (dependency.myImpl->try_attach_child(myImpl)) {

	}
}

}
