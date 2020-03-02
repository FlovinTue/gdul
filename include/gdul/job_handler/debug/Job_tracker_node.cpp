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