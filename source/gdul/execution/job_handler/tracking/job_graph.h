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

#include <gdul/execution/job_handler/globals.h>
#include <gdul/execution/job_handler/tracking/job_info.h>

#include <gdul/containers/concurrent_unordered_map.h>

namespace gdul {
namespace jh_detail {

class job_graph
{
public:
	job_graph();

	job_info* fetch_job_info(std::size_t id);

#if defined (GDUL_JOB_DEBUG)
	job_info* get_job_info(std::size_t physicalId, std::size_t variationId, const std::string_view& name, const std::string_view& file, std::uint32_t line);
	job_info* get_sub_job_info(std::size_t batchId, std::size_t variationId, const std::string_view& name);

	void dump_job_graph(const std::string_view& location);
	void dump_job_time_sets(const std::string_view& location);
#else
	job_info* get_job_info(std::size_t physicalId, std::size_t variationId);
	job_info* get_sub_job_info(std::size_t batchId, std::size_t variationId);
#endif

private:
	concurrent_unordered_map<std::uint64_t, job_info> m_map;
};
}
}