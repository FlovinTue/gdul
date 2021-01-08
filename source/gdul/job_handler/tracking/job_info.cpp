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
#include <memory>

namespace gdul
{
namespace jh_detail
{
job_info::job_info()
	: m_id(0)
	, m_lastAccumulatedRuntime(0.f)
	, m_accumulatedRuntime(0.f)
	, m_runtime(0.f)

#if defined(GDUL_JOB_DEBUG)
	, m_name()
	, m_physicalLocation()
	, m_line(0)
	, m_parent(0)
	, m_type(job_default)
#endif
{
}
job_info::job_info(job_info&& other)
{
	operator=(other);
}
job_info::job_info(const job_info& other)
{
	operator=(other);
}
job_info& job_info::operator=(job_info&& other)
{
	operator=(other);
	return *this;
}
job_info& job_info::operator=(const job_info& other)
{
#if defined(GDUL_JOB_DEBUG)
	m_name = other.m_name;
	m_physicalLocation = other.m_physicalLocation;

	m_line = other.m_line;
#endif

	m_id = other.m_id;

	m_lastAccumulatedRuntime = other.m_lastAccumulatedRuntime.load();
	m_accumulatedRuntime = other.m_accumulatedRuntime.load();

	m_runtime = other.m_runtime;

#if defined(GDUL_JOB_DEBUG)
	m_parent = other.m_parent;

	m_type = other.m_type;
#endif
	return *this;
}
void job_info::accumulate_runtime(float priority)
{
	float priorityAccumulation(m_accumulatedRuntime.load(std::memory_order_relaxed));

	while (priorityAccumulation < priority) {
		if (m_accumulatedRuntime.compare_exchange_strong(priorityAccumulation, priority, std::memory_order_relaxed)) {
			break;
		}
	}
}

void job_info::store_runtime(float runtime)
{
	m_runtime = runtime;

	float priorityAccumulation(m_accumulatedRuntime.exchange(0.f, std::memory_order_relaxed));

	if (priorityAccumulation != 0.f) {
		m_lastAccumulatedRuntime.store(priorityAccumulation, std::memory_order_relaxed);
	}
}
float job_info::get_dependant_runtime() const
{
	return m_lastAccumulatedRuntime.load(std::memory_order_relaxed);
}
float job_info::get_runtime() const
{
	return m_runtime;
}
std::size_t job_info::id() const
{
	return m_id;
}

#if defined(GDUL_JOB_DEBUG)
std::size_t job_info::parent() const
{
	return m_parent;
}
void job_info::set_job_type(job_type type)
{
	m_type = type;
}

job_type job_info::get_node_type() const
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
#endif

}
}