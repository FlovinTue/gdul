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
#include <filesystem>

namespace gdul {
namespace jh_detail {
void write_node(const job_tracker_node& node, std::string& outNodes, std::string& outLinks);


concurrency::concurrent_unordered_map<std::uint64_t, job_tracker_node> g_jobTrackingNodeMap;
thread_local std::string t_bufferString;

job_tracker_node* job_tracker::register_node(constexpr_id id, const char * name, const char* file, std::uint32_t line)
{
	t_bufferString = name;

	if (t_bufferString.empty()){
		t_bufferString = "Unnamed";
	}

	const std::uint64_t variationHash(std::hash<std::string>{}(t_bufferString));

	const constexpr_id groupParent(job_handler::this_job.m_debugId);
	const constexpr_id groupMatriarch(id);
	const constexpr_id localId(variationHash + groupMatriarch.value());

	auto itr = g_jobTrackingNodeMap.insert({ localId.value(), job_tracker_node() });
	if (itr.second){
		itr.first->second.m_id = id;
		itr.first->second.m_parent = groupMatriarch;
		itr.first->second.m_name = name;

		auto matriarchItr = g_jobTrackingNodeMap.insert({ groupMatriarch.value(), job_tracker_node() });
		if (matriarchItr.second){
			matriarchItr.first->second.m_id = groupMatriarch;
			matriarchItr.first->second.m_parent = groupParent;
			matriarchItr.first->second.set_node_type(job_tracker_node_matriarch);
			std::string matriarchName;
			matriarchName.append(std::filesystem::path(file).filename().string());
			matriarchName.append("__L:_");
			matriarchName.append(std::to_string(line));
			matriarchName.append("__Id:_");
			matriarchName.append(std::to_string(id.value()));
			matriarchItr.first->second.m_name = std::move(matriarchName);

			auto groupParentItr = g_jobTrackingNodeMap.insert({ groupParent.value(), job_tracker_node() });
			if (groupParentItr.second){
				groupParentItr.first->second.m_id = groupParent;

				groupParentItr.first->second.set_node_type(job_tracker_node_matriarch);
				groupParentItr.first->second.m_name = std::move(matriarchName);
				
			}
		}
	}
	return &itr.first->second;
}
void job_tracker::dump_job_tree(const char* location)
{
	const std::string folder(location);
	const std::string programName("myprogram");
	const std::string outputFile(folder + programName + "_" + "job_graph.dgml");
	
	std::string nodes;
	std::string links;

	for (auto node : g_jobTrackingNodeMap){
		write_node(node.second, nodes, links);
	}

	std::ofstream outStream;
	outStream.open(outputFile,std::ofstream::out);
	
	outStream << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
	outStream << "<DirectedGraph Title=\"DrivingTest\" Background=\"Grey\" xmlns=\"http://schemas.microsoft.com/vs/2009/dgml\">\n";

	outStream << "<Nodes>\n";
	outStream << std::move(nodes);
	outStream << "</Nodes>\n";

	outStream << "<Links>\n";
	outStream << std::move(links);
	outStream << "</Links>\n";
	outStream << "</DirectedGraph>\n";

	outStream.close();
}

void write_node(const job_tracker_node& node, std::string& outNodes, std::string& outLinks)
{
	t_bufferString.clear();

	t_bufferString.append("<Node Id=\"");
	t_bufferString.append(std::to_string(node.id().value()));
	t_bufferString.append("\"");
	t_bufferString.append(" Label=\"");
	t_bufferString.append(node.name());
	t_bufferString.append("\"");

	if (node.get_node_type() == job_tracker_node_matriarch){
		t_bufferString.append(" Group=\"Expanded\"");
	}

	t_bufferString.append("  />");
	t_bufferString.append("\n");

	outNodes.append(t_bufferString);

	t_bufferString.clear();
	t_bufferString.append("<Link");

	if (node.get_node_type() == job_tracker_node_default){
		t_bufferString.append(" Category=\"Contains\"");
	}
	t_bufferString.append(" Source=\"");
	t_bufferString.append(std::to_string(node.parent().value()));
	t_bufferString.append("\" Target = \"");
	t_bufferString.append(std::to_string(node.id().value()));
	t_bufferString.append("\"");

	t_bufferString.append("  />");
	t_bufferString.append("\n");

	outLinks.append(t_bufferString);
}
}
}
#endif