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

#if defined (GDUL_JOB_DEBUG)

#include <gdul/job_handler/debug/job_tracker_node.h>


#endif

namespace gdul {
namespace jh_detail {

class job_tracker
{
public:
#if defined (GDUL_JOB_DEBUG)
	static job_tracker_node* register_full_node(std::size_t id, const char * name, const char* file, std::uint32_t line);
	static job_tracker_node* register_batch_sub_node(std::size_t id, const char* name);

	static job_tracker_node* fetch_node(std::size_t id);

	static void dump_job_tree(const char* location);
	static void dump_job_time_sets(const char* location);
#else
	static job_tracker_node* register_full_node(std::size_t id);
	static job_tracker_node* register_batch_sub_node(std::size_t id);

	static job_tracker_node* fetch_node(std::size_t id);

	static void dump_job_tree(const char*) {};
	static void dump_job_time_sets(const char*) {};
#endif
};
}
}