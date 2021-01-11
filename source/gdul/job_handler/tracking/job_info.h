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
#include <atomic>

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/tracking/time_set.h>
#include <string>
#endif


namespace gdul
{
namespace jh_detail
{
#if defined (GDUL_JOB_DEBUG)
enum job_type : std::uint8_t
{
	job_default,
	job_batch,
	job_physical,
};
#endif
struct job_info
{
	job_info();
	job_info(job_info&& other);
	job_info(const job_info& other);
	job_info& operator=(job_info&& other);
	job_info& operator=(const job_info& other);


	void accumulate_dependant_time(float priority);
	void accumulate_propagation_time(float priority);
	float get_dependant_runtime() const;
	float get_propagation_runtime() const;
	float get_runtime() const;
	void store_runtime(float runtime);

	std::size_t id() const;

#if defined(GDUL_JOB_DEBUG)
	std::size_t parent() const;

	void set_job_type(job_type type);
	job_type get_node_type() const;

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

	std::size_t m_id;

	std::atomic<float> m_lastDependantRuntime;
	std::atomic<float> m_dependantRuntime;
	std::atomic<float> m_lastAccumulatedPropagationTime;
	std::atomic<float> m_propagationTime;

	float m_runtime;

#if defined(GDUL_JOB_DEBUG)
	std::string m_name;
	std::string m_physicalLocation;

	std::uint32_t m_line;

	std::size_t m_parent;

	job_type m_type;
#endif
};
}
}
