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
#include <gdul/job_handler/tracking/job_info.h>

namespace gdul {
namespace jh_detail {

class job_graph
{
public:
	static job_info* fetch_job_info(std::size_t id);

#if defined (GDUL_JOB_DEBUG)
	static job_info* get_job_info(std::size_t physicalId, std::size_t variationId, const char * name, const char* file, std::uint32_t line);
	static job_info* get_job_info_sub(std::size_t batchId, std::size_t variationId, const char* name);

	static void dump_job_tree(const char* location);
	static void dump_job_time_sets(const char* location);
#else
	static job_info* get_job_info(std::size_t physicalId, std::size_t variationId);
	static job_info* get_job_info_sub(std::size_t batchId, std::size_t variationId);

	static void dump_job_tree(const char*) {};
	static void dump_job_time_sets(const char*) {};
#endif
};
}
}