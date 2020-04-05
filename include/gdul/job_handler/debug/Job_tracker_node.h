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

#include <gdul/job_handler/debug/constexp_id.h>
#include <gdul/job_handler/debug/time_set.h>
#include <string>

namespace gdul
{
namespace jh_detail
{
enum job_tracker_node_type : std::uint8_t
{
	job_tracker_node_default,
	job_tracker_node_batch,
	job_tracker_node_matriarch,
};
struct job_tracker_node
{
	job_tracker_node();

	constexpr_id id() const;
	constexpr_id parent() const;

	void set_node_type(job_tracker_node_type type);
	job_tracker_node_type get_node_type() const;

	const std::string& name() const;

	time_set m_completionTimeSet;
	time_set m_waitTimeSet;
	time_set m_enqueueTimeSet;

private:
	friend class job_tracker;
	friend class job_tracker_data;

	std::string m_name;

	constexpr_id m_id;
	constexpr_id m_parent;

	job_tracker_node_type m_type;
};
}
}

#endif