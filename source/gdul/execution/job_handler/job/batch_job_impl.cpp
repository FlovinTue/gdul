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

#include "batch_job_impl.h"
#include <gdul/utility/delegate.h>
#include <gdul/execution/job_handler/job_handler.h>
#include <gdul/execution/job_handler/job_handler_impl.h>
#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/job_handler/job/job_impl.h>

namespace gdul
{
namespace jh_detail
{
gdul::job _redirect_make_job(job_handler_impl* handler, gdul::delegate<void()>&& workUnit, job_queue* target, std::size_t batchId, std::size_t variationId, [[maybe_unused]] const std::string_view& name)
{
#if defined (GDUL_JOB_DEBUG)
	return handler->make_sub_job_internal(std::move(workUnit), target, batchId, variationId, name);
#else
	return handler->make_sub_job_internal(std::move(workUnit), target, batchId, variationId);
#endif
}
bool _redirect_enable_if_ready(gdul::shared_ptr<job_impl>& jb)
{
	return jb->enable_if_ready();
}
void _redirect_invoke_job(gdul::shared_ptr<job_impl>& jb)
{
	GDUL_JOB_DEBUG_CONDTIONAL(jb->on_enqueue())
	jb->operator()();
}
bool _redirect_is_enabled(const gdul::shared_ptr<job_impl>& jb)
{
	return jb->is_enabled();
}
void _redirect_set_info(shared_ptr<job_handler_impl>& handler, gdul::shared_ptr<job_impl>& jb, std::size_t physicalId, std::size_t variationId, [[maybe_unused]] const std::string_view& name)
{
#if defined (GDUL_JOB_DEBUG)
	jb->set_info(handler->get_job_graph().get_sub_job_info(physicalId, variationId, name));
#else
	jb->set_info(handler->get_job_graph().get_sub_job_info(physicalId, variationId));
#endif
}
}
}
