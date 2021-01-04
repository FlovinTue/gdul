#include "job_queue.h"
#include <gdul/job_handler/job_impl.h>

float gdul::job_queue::priority(const jh_detail::job_impl_shared_ptr& of) const
{
    return of->get_priority();
}
