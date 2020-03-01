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
#include <gdul/job_handler/debug/job_tracker.h>
#endif




namespace gdul {
namespace jh_detail {
class job_tracker_interface
{
public:
#if !defined(GDUL_JOB_DEBUG)
	void activate_job_tracking(const char*) {}
#else
	void activate_job_tracking(const char* name) {(void)name;}
	job_tracker_interface()
		: m_physicalId(constexpr_id::make<0>())
	{}

	virtual ~job_tracker_interface() {}

	void activate_job_tracking(
		constexpr_id id, 
		const char* name,
		const char* file,
		uint32_t line) {

		m_physicalId = register_tracking_node(id, name, file, line);
	}

protected:
	virtual constexpr_id register_tracking_node(
		constexpr_id id,
		const char* name,
		const char* file,
		std::uint32_t line, bool batchSub = false) = 0;

private:
	friend class job_tracker;
	friend class job_impl;
	template <class InputContainer, class OutputContainer, class Process>
	friend class batch_job_impl;

	constexpr_id m_physicalId;
#endif
};
}
}

#if defined(GDUL_JOB_DEBUG)
#if !defined (activate_job_tracking)
#if  defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define GDUL_INLINE_PRAGMA(pragma) __pragma(pragma)
#else
#define GDUL_STRINGIFY_PRAGMA(pragma) #pragma
#define GDUL_INLINE_PRAGMA(pragma) _Pragma(GDUL_STRINGIFY_PRAGMA(pragma))
#endif
// A little trick to squeeze in a macro. To enable compile time enumeration of declarations
#define activate_job_tracking(name) activate_job_tracking(gdul::constexpr_id::make< \
GDUL_INLINE_PRAGMA(warning(push)) \
GDUL_INLINE_PRAGMA(warning(disable : 4307)) \
constexp_str_hash(__FILE__) \
GDUL_INLINE_PRAGMA(warning(pop)) \
+ std::size_t(__LINE__)>(), \
name, \
__FILE__, \
__LINE__)
#endif
#endif