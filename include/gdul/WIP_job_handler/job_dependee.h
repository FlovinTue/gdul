#pragma once

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul
{
namespace jh_detail
{

class job_impl;

struct job_dependee
{
	gdul::shared_ptr<job_dependee> m_sibling;
	gdul::shared_ptr<job_impl> m_job;
};
using job_dependee_shared_ptr = gdul::shared_ptr<job_dependee>;
using job_dependee_atomic_shared_ptr = gdul::atomic_shared_ptr<job_dependee>;
using job_dependee_raw_ptr = gdul::raw_ptr<job_dependee>;
}
}