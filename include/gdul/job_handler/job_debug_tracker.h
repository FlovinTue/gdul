// Copyright(c) 2019 Flovin Michaelsen
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

#include <type_traits>

// https://stackoverflow.com/questions/48896142/is-it-possible-to-get-hash-values-as-compile-time-constants
template <typename Str>
constexpr size_t constexp_str_hash(const Str& toHash)
{
	size_t result = 0xcbf29ce484222325ull; 

	for (char c : toHash) {
		result ^= c;
		result *= 1099511628211ull;
	}

	return result;
}

namespace gdul {
class job;
class batch_job;

struct constexpr_id
{
template <std::uint64_t Id>
static constexpr constexpr_id make() {
	return constexpr_id(Id);
}

std::uint64_t value() const noexcept {return m_val;}

private:
	std::uint64_t m_val;

	constexpr constexpr_id(std::uint64_t id)
		: m_val(id)
	{}
};

namespace jh_detail {


class job_debug_tracker
{
public:
	static void register_node(constexpr_id id);
	static void add_node_variation(constexpr_id id, const char* name);

	static void dump_job_tree(const char* location);

};
}
}
#endif