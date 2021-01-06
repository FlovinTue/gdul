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

#include <gdul/job_handler/tracking/job_info.h>


namespace gdul
{
namespace jh_detail
{
job_info::job_info()
	: m_id(0)
	, m_lastPriorityAccum(0.f)
	, m_priorityAccum(0.f)
#if !defined (GDUL_JOB_DEBUG)
	, m_lastCompletionTime(0.f)
#else
	, m_parent(0)
	, m_type(job_info_default)
	, m_line(0)
#endif
{
}
void job_info::accumulate_priority(job_info* from)
{
	if (!from)
		return;

	const float prio(from->get_priority());

	if (m_priorityAccum < prio)
		m_priorityAccum = prio;
}

void job_info::store_accumulated_priority(float lastCompletionTime)
{
	m_lastCompletionTime = lastCompletionTime;

	const float priorityAccumulation(m_priorityAccum);

	if (priorityAccumulation != 0.f) {
		m_lastPriorityAccum = priorityAccumulation;
		m_priorityAccum = 0.f;
	}
}
std::size_t job_info::id() const
{
	return m_id;
}

#if defined(GDUL_JOB_DEBUG)
float job_info::get_priority() const
{
	return m_lastPriorityAccum + m_completionTimeSet.get_avg();
}
std::size_t job_info::parent() const
{
	return m_parent;
}
void job_info::set_node_type(job_info_type type)
{
	m_type = type;
}

job_info_type job_info::get_node_type() const
{
	return m_type;
}

const std::string & job_info::name() const
{
	return m_name;
}

const std::string& job_info::physical_location() const
{
	return m_physicalLocation;
}

std::uint32_t job_info::line() const
{
	return m_line;
}
#else
float job_info::get_priority() const
{
	return m_lastPriorityAccum + m_lastCompletionTime;
}
#endif

}
}