#include "job_debug_tracker.h"

#if defined (GDUL_JOB_DEBUG)
#include <concurrent_unordered_map.h>
#include <gdul/job_handler/job_handler.h>
#include <unordered_set>

namespace gdul {
namespace jh_detail {

struct job_debug_tracking_node
{
	job_debug_tracking_node()
		: m_id(constexpr_id::make<0>())
	{}
	constexpr_id m_id;
	std::unordered_set<std::uint64_t> m_children;
	std::unordered_set<std::string> m_variations;
};

concurrency::concurrent_unordered_map<std::uint64_t, job_debug_tracking_node> g_jobTrackingNodeMap;

thread_local job_debug_tracking_node* t_lastNode;

void job_debug_tracker::register_node(constexpr_id id)
{
	g_jobTrackingNodeMap[id.value()];

	if (job_handler::this_job.m_debugId.value() != 0) {
		g_jobTrackingNodeMap[job_handler::this_job.m_debugId.value()].m_children.insert(id.value());
	}
}
void job_debug_tracker::add_node_variation(constexpr_id id, const char * name)
{
	if (!t_lastNode || t_lastNode->m_id.value() != id.value()) {
		t_lastNode = &g_jobTrackingNodeMap[id.value()];
	}
	std::string variation(name);

	t_lastNode->m_variations.insert(std::move(variation));
}
void job_debug_tracker::dump_job_tree(const char* location)
{
	location;

	std::vector<job_debug_tracking_node> nodes;

	for (auto itr = g_jobTrackingNodeMap.begin(); itr != g_jobTrackingNodeMap.end(); ++itr) {
		nodes.push_back(itr->second);
	}

	std::vector<std::pair<std::uint64_t, std::uint64_t>> connections;

	for (auto& node : nodes) {
		for (auto& child : node.m_children) {
			connections.push_back({ node.m_id.value(), child });
		}
	}
}
}
}
#endif