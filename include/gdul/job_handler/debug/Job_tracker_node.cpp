#include "Job_tracker_node.h"

#if defined(GDUL_JOB_DEBUG)

namespace gdul
{
namespace jh_detail
{
job_tracker_node::job_tracker_node()
	: m_id(constexpr_id::make<0>())
	, m_parent(constexpr_id::make<0>())
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
constexpr_id job_tracker_node::parent() const
{
	return m_parent;
}
void job_tracker_node::add_completion_time(float time)
{
	++m_completedCount;
	m_minTime = time < m_minTime ? time : m_minTime;
	m_maxTime = m_maxTime < time ? time : m_maxTime;
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