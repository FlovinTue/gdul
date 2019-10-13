#include "job_impl.h"
#include "job_callable_base.h"

namespace gdul {
namespace job_handler_detail {

job_impl::job_impl()
{
}


job_impl::~job_impl()
{
}

void job_handler_detail::job_impl::operator()()
{
	(*myCallable)();
}
}
}
