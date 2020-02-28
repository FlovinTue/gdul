#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)

#include <gdul/job_handler/debug/constexp_id.h>
#include <string>

namespace gdul
{
namespace jh_detail
{
enum job_tracker_node_type : std::uint8_t
{
	job_tracker_node_default,
	job_tracker_node_batch,
	job_tracker_node_batch_sub,
	job_tracker_node_matriarch,
};
struct job_tracker_node
{
	job_tracker_node();

	constexpr_id id() const;
	constexpr_id parent() const;

	void add_completion_time(float time);

	float min_time() const;
	float max_time() const;
	float avg_time() const;

	std::size_t completed_count() const;

	void set_node_type(job_tracker_node_type type);
	job_tracker_node_type get_node_type() const;

	const std::string& name() const;

private:
	friend class job_tracker;
	friend class job_tracker_data;

	std::string m_name;

	constexpr_id m_id;
	constexpr_id m_parent;

	std::size_t m_completedCount;

	float m_minTime;
	float m_avgTime; 
	float m_maxTime;

	job_tracker_node_type m_type;
};
}
}

#endif