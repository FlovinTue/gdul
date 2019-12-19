#include "scatter_job_impl.h"
#include <gdul/delegate/delegate.h>
#include <gdul/WIP_job_handler/job_handler.h>

namespace gdul
{
namespace jh_detail
{
job gdul::jh_detail::_redirect_make_job(job_handler * handler, gdul::delegate<void()>&& workUnit)
{
	return handler->make_job(std::move(workUnit));
}
}
}
