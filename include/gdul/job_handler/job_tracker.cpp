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

#include "job_tracker.h"

#if defined (GDUL_JOB_DEBUG)
#include <concurrent_unordered_map.h>
#include <gdul/job_handler/job_handler.h>
#include <unordered_set>
#include <fstream>
#include <atomic>

namespace gdul {
namespace jh_detail {


concurrency::concurrent_unordered_map<std::uint64_t, job_tracking_node> g_jobTrackingNodeMap;
thread_local std::string t_bufferString;

job_tracking_node* job_tracker::register_node(constexpr_id id, const char * name)
{
	t_bufferString = name;

	const std::uint64_t variationHash(std::hash<std::string>{}(t_bufferString));

	const constexpr_id groupParent(job_handler::this_job.m_debugId);
	const constexpr_id groupMatriarch(id);
	const constexpr_id localId(variationHash + groupMatriarch.value());

	auto itr = g_jobTrackingNodeMap.insert({ localId.value(), job_tracking_node() });
	if (itr.second){
		itr.first->second.m_id = id;
		itr.first->second.m_parent = groupMatriarch;
		itr.first->second.m_name = name;

		auto matriarchItr = g_jobTrackingNodeMap.insert({ groupMatriarch.value(), job_tracking_node() });
		if (matriarchItr.second){
			matriarchItr.first->second.m_id = groupMatriarch;
			matriarchItr.first->second.m_parent = groupParent;
			matriarchItr.first->second.m_name = std::string("Job#").append(std::to_string(id.value()));

			auto groupParentItr = g_jobTrackingNodeMap.insert({ groupParent.value(), job_tracking_node() });
			if (groupParentItr.second){
				groupParentItr.first->second.m_id = groupParent;
			}
		}
	}
	return &itr.first->second;
}
void job_tracker::dump_job_tree(const char* location)
{
	location;
	//const std::string folder(location);
	//const std::string programName("myprogram");
	//const std::string outputFile(folder + programName + "_" + "job_graph.dgml");
	//
	//std::ofstream outStream;
	//outStream.open(outputFile,std::ofstream::out);
	//
	//outStream << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
	//outStream << "<DirectedGraph Title=\"DrivingTest\" Background=\"Grey\" xmlns=\"http://schemas.microsoft.com/vs/2009/dgml\">\n";
	//
	//std::vector<job_tracking_node> nodes;
	//
	//for (auto itr = g_jobTrackingNodeMap.begin(); itr != g_jobTrackingNodeMap.end(); ++itr) {
	//	nodes.push_back(itr->second);
	//}

	//std::vector<std::pair<std::uint64_t, std::uint64_t>> connections;

	//for (auto& node : nodes) {
	//	for (auto& child : node.m_children) {
	//		connections.push_back({ node.m_id.value(), child });
	//	}
	//}

	//outStream << "<Nodes>\n";
	//for (std::size_t i = 0; i < nodes.size(); ++i) {
	//	outStream << "<Node Id=\"" << nodes[i].m_id.value() << "\" Label=\"" << *nodes[i].m_variations.begin() << "\" Category=\"Person\" />\n";
	//}
	//outStream << "</Nodes>\n";
	//outStream << "<Links>\n";
	//for (std::size_t i = 0; i < connections.size(); ++i) {
	//	outStream << "<Link Source=\""<< connections[i].first << "\" Target=\""<< connections[i].second <<"\"/>\n";
	//}
	//outStream << "</Links>\n";
	//outStream << "</DirectedGraph>\n";
	//outStream.close();
}
}
}
#endif