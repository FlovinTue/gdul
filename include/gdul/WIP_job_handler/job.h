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

namespace gdul {

// Jobs are thread-safe to depend on?
// Thread A job, depends on thread B job. Needs atomic_shared_ptr... 
// Depend on job, or depend on job_impl ? 
// Depend on job, and return job_impl to pool?
namespace job_handler_detail {

class job_impl;
}
class job
{
public:
	job();

	template <class Callable>
	job(Callable&& callable);
	template <class Callable>
	job(Callable&& callable, std::uint8_t priority);
	template <class Callable>
	job(Callable&& callable, const job& dependency);
	template <class Callable>
	job(Callable&& callable, const job& dependency, std::uint8_t priority);

	~job();

	job(job&& other);
	job& operator=(job&& other);

	job(const job&) = delete;
	job& operator=(const job&) = delete;

private:
	atomic_shared_ptr<job_handler_detail::job_impl> myImpl;
};

}
