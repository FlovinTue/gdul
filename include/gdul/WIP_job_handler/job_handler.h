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

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/worker.h>
#include <gdul/WIP_job_handler/job.h>

#pragma once

namespace gdul
{

namespace jh_detail
{
class job_handler_impl;
class job_delegate;
}



// Should think about the way delegates & jobs are created. Want to keep creation intuitive & simple
class job_handler
{
public:
	job_handler();
	job_handler(const jh_detail::allocator_type& allocator);
	~job_handler();

	void retire_workers();

	worker make_worker();

	// Callable will allocate if size (Callable + stored arguments) is above 
	// jh_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	// Priority corresponds to the internal queue it will be placed in. Priority 0 -> highest. 
	// jh_detail::Priority_Granularity defines the number of queueus
	template <class Callable, class ...Args>
	job make_job(Callable&& call, std::uint8_t priority, Args&& ...args);

	// Callable will allocate if size (Callable + stored arguments) is above 
	// jh_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	template <class Callable, class ...Args>
	job make_job(Callable&& call, Args&& ...args);

	std::size_t num_workers() const;
	std::size_t num_enqueued() const;
private:
	job make_job_internal(const jh_detail::job_delegate& call, std::uint8_t priority);

	gdul::shared_ptr<jh_detail::job_handler_impl> m_impl;
	jh_detail::allocator_type m_allocator;
};
template<class Callable, class ...Args>
inline job job_handler::make_job(Callable && call, std::uint8_t priority, Args&& ...args)
{
	return make_job_internal(jh_detail::job_delegate(std::forward<Callable&&>(call), m_allocator, std::forward<Args&&>(args)...), priority);
}
template<class Callable, class ...Args>
inline job job_handler::make_job(Callable && call, Args&& ...args)
{
	return make_job(std::forward<Callable&&>(call), jh_detail::Default_Job_Priority, std::forward<Args&&>(args)...);
}
}