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

#include <gdul/job_handler/debug/job_tracker_node.h>

#if defined(GDUL_JOB_DEBUG)

namespace gdul
{
namespace jh_detail
{
job_tracker_node::job_tracker_node()
	: m_id(constexpr_id::make<0>())
	, m_parent(constexpr_id::make<0>())
	, m_type(job_tracker_node_default)
	, m_line(0)
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

const std::string& job_tracker_node::physical_location() const
{
	return m_physicalLocation;
}

std::uint32_t job_tracker_node::line() const
{
	return m_line;
}

}
}
#endif