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

#include "job_debug_tracker.h"

#if defined (GDUL_JOB_DEBUG)
#include <concurrent_unordered_map.h>
#include <gdul/job_handler/job_handler.h>
#include <unordered_set>
#include <fstream>
#include <atomic>

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
	std::uint32_t m_createdIndex;
};

concurrency::concurrent_unordered_map<std::uint64_t, job_debug_tracking_node> g_jobTrackingNodeMap;
std::atomic_uint32_t g_createdIndexer = 0;

thread_local job_debug_tracking_node* t_lastNode;

void job_debug_tracker::register_node(constexpr_id id)
{
	auto& entry(g_jobTrackingNodeMap[id.value()]);
	entry.m_id = id;
	entry.m_createdIndex = g_createdIndexer.fetch_add(1);

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
	const std::string folder(location);
	const std::string programName("myprogram");
	const std::string outputFile(folder + programName + "_" + "job_graph.dgml");

	std::ofstream outStream;
	outStream.open(outputFile,std::ofstream::out);

	outStream << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
	outStream << "<DirectedGraph Title=\"DrivingTest\" Background=\"Grey\" xmlns=\"http://schemas.microsoft.com/vs/2009/dgml\">\n";

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

	outStream << "<Nodes>\n";
	for (std::size_t i = 0; i < nodes.size(); ++i) {
		outStream << "<Node Id=\"" << nodes[i].m_id.value() << "\" Label=\"" << *nodes[i].m_variations.begin() << "\" Category=\"Person\" />\n";
	}
	outStream << "</Nodes>\n";
	outStream << "<Links>\n";
	for (std::size_t i = 0; i < connections.size(); ++i) {
		outStream << "<Link Source=\""<< connections[i].first << "\" Target=\""<< connections[i].second <<"\"/>\n";
	}
	outStream << "</Links>\n";
	outStream << "</DirectedGraph>\n";
	outStream.close();
}
}
}
#endif