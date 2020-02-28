#include "Job_tracker_node.h"

#if defined(GDUL_JOB_DEBUG)

#include <algorithm>

namespace gdul
{
namespace jh_detail
{
job_tracker_node::job_tracker_node()
	: m_id(constexpr_id::make<0>())
	, m_groupParent(constexpr_id::make<0>())
	, m_matriarch(constexpr_id::make<0>())
	, m_completedCount(0)
	, m_minTime(0.f)
	, m_avgTime(0.f)
	, m_maxTime(0.f)
	, m_type(job_tracker_node_default)
{
}
constexpr_id job_tracker_node::id() const
{
	return m_id;
}
constexpr_id job_tracker_node::group_parent() const
{
	return m_groupParent;
}
constexpr_id job_tracker_node::group_matriarch() const
{
	return m_matriarch;
}
void job_tracker_node::add_completion_time(float time)
{
	++m_completedCount;
	m_minTime = std::min(m_minTime, time);
	m_maxTime = std::max(m_maxTime, time);
	m_avgTime += time;
}

float job_tracker_node::min_time() const
{
	return m_minTime;
}

float job_tracker_node::max_time() const
{
	return m_maxTime;
}

float job_tracker_node::avg_time() const
{
	return m_avgTime / m_completedCount;
}

std::size_t job_tracker_node::completed_count() const
{
	return m_completedCount;
}

void job_tracker_node::set_node_type(job_tracker_node_type type)
{
	m_type = type;
}

job_tracker_node_type job_tracker_node::get_node_type() const
{
	return m_type;
}

const std::string & job_tracker_node::name() const
{
	return m_name;
}

}
}
#endif