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

#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/tracking/time_set.h>
#include <string>
#endif


namespace gdul
{
namespace jh_detail
{
#if defined (GDUL_JOB_DEBUG)
enum job_info_type : std::uint8_t
{
	job_info_default,
	job_info_batch,
	job_info_matriarch,
};
#endif
struct job_info
{
	job_info();

	void accumulate_priority(job_info* from);
	float get_priority() const;
	void store_accumulated_priority(float lastCompletionTime);

	std::size_t id() const;

#if defined(GDUL_JOB_DEBUG)
	std::size_t parent() const;

	void set_node_type(job_info_type type);
	job_info_type get_node_type() const;

	const std::string& name() const;
	const std::string& physical_location() const;

	std::uint32_t line() const;

	time_set m_completionTimeSet;
	time_set m_waitTimeSet;
	time_set m_enqueueTimeSet;
#endif

private:
	friend class job_graph;
	friend class job_graph_data;

#if defined(GDUL_JOB_DEBUG)
	std::string m_name;
	std::string m_physicalLocation;

	std::uint32_t m_line;
#endif

	std::size_t m_id;

	float m_lastPriorityAccum;
	float m_priorityAccum;
	float m_lastCompletionTime;

#if defined(GDUL_JOB_DEBUG)
	std::size_t m_parent;

	job_info_type m_type;
#endif
};
}
}
