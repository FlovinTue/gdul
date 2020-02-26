#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)

#include <gdul/job_handler/debug/constexp_id.h>
#include <string>

namespace gdul
{
namespace jh_detail
{
struct job_tracker_node
{
	job_tracker_node()
		: m_id(constexpr_id::make<0>())
		, m_parent(constexpr_id::make<0>())
	{}

	constexpr_id id() const;
	constexpr_id parent() const;


private:
	friend class job_tracker;

	std::string m_name;

	float m_minTime;
	float m_avgTime; // Imlement average time neatly?
	float m_maxTime;

	constexpr_id m_id;
	constexpr_id m_parent;
};
}
}

#endif