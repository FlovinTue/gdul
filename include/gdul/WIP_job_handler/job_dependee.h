#pragma once

#include <gdul/WIP_job_handler/job_impl.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul
{
namespace job_handler_detail
{
struct job_dependee
{
	atomic_shared_ptr<job_dependee> m_next;
	shared_ptr<job_impl> m_dependee;
};
}
}