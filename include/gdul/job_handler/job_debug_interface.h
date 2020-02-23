#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)
#include <gdul/job_handler/job_debug_tracker.h>
#endif

namespace gdul {
namespace jh_detail {
class job_debug_interface
{
public:
	void activate_debug_tracking(const char* name) {
		(void)name;
	}
#if defined(GDUL_JOB_DEBUG)
	job_debug_interface()
		: m_debugId(constexpr_id::make<0>())
	{}

	virtual ~job_debug_interface() {}

	void activate_debug_tracking(const char* name, constexpr_id id) {
		register_debug_node(name, id);
		m_debugId = id;
	}

protected:
	virtual void register_debug_node(const char* name, constexpr_id id) noexcept { name; id; };
private:
	friend class job_debug_tracker;

	constexpr_id m_debugId;
#endif
};
}
}

// A little trick to squeeze in a macro. To enable compile time enumeration of declarations
#if !defined (activate_debug_tracking)
#if defined(GDUL_JOB_DEBUG)
#define activate_debug_tracking(name) activate_debug_tracking(name, GDUL_CEXPR_ID())
#else
#define activate_debug_tracking(name) activate_debug_tracking(name)
#endif
#endif