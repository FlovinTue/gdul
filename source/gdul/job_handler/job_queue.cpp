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

#include "job_queue.h"
#include <gdul/job_handler/job/job_impl.h>

namespace gdul {

void job_async_queue::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::move(jb));
}
job_async_queue::job_async_queue(jh_detail::allocator_type alloc)
	: m_queue(alloc)
{
}
jh_detail::job_impl_shared_ptr job_async_queue::fetch_job()
{
	jh_detail::job_impl_shared_ptr out;
	m_queue.try_pop(out);
	return out;
}
job_sync_queue::job_sync_queue(jh_detail::allocator_type alloc)
	: m_queue(alloc)
{
}
void job_sync_queue::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::make_pair(jb->get_priority(), std::move(jb)));
}
jh_detail::job_impl_shared_ptr job_sync_queue::fetch_job()
{
	std::pair<float, jh_detail::job_impl_shared_ptr> out;
	m_queue.try_pop(out);
	return out.second;
}
}