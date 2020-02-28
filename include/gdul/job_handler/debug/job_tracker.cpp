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
#include <unordered_map>
#include <fstream>
#include <atomic>
#if (_HAS_CXX17)
#include <filesystem>
#define GDUL_FS_PATH(arg) std::filesystem::path(arg)
#else
#include <experimental/filesystem>
#define GDUL_FS_PATH(arg) std::experimental::filesystem::path(arg)
#endif

namespace gdul {
namespace jh_detail {
void write_node(const job_tracker_node& node, const std::unordered_map<std::uint64_t, std::size_t>& childCounter, std::string& outNodes, std::string& outLinks);

class job_tracker_data
{
public:
	job_tracker_data()
	{
		auto itr = m_nodeMap.insert({ 0, job_tracker_node() });
		itr.first->second.m_name = "Undeclared_Root_Node";
		itr.first->second.m_type = job_tracker_node_default;
	}
	concurrency::concurrent_unordered_map<std::uint64_t, job_tracker_node> m_nodeMap;
}g_trackerData;

thread_local std::string t_bufferString;

job_tracker_node* job_tracker::register_full_node(constexpr_id id, const char * name, const char* file, std::uint32_t line)
{
	t_bufferString = name;

	if (t_bufferString.empty()){
		t_bufferString = "Unnamed";
	}

	const constexpr_id variation(std::hash<std::string>{}(t_bufferString));

	const constexpr_id groupParent(job_handler::this_job.m_debugId);
	const constexpr_id groupMatriarch(id.merge(groupParent));
	const constexpr_id localId(variation.merge(groupMatriarch));

	auto itr = g_trackerData.m_nodeMap.insert({ localId.value(), job_tracker_node() });
	if (itr.second){
		itr.first->second.m_id = localId;
		itr.first->second.m_parent = groupMatriarch;
		itr.first->second.m_name = std::move(t_bufferString);

		auto matriarchItr = g_trackerData.m_nodeMap.insert({ groupMatriarch.value(), job_tracker_node() });
		if (matriarchItr.second){
			matriarchItr.first->second.m_id = groupMatriarch;
			matriarchItr.first->second.m_parent = groupParent;
			matriarchItr.first->second.set_node_type(job_tracker_node_matriarch);
			std::string matriarchName;
			matriarchName.append(GDUL_FS_PATH(file).filename().string());
			matriarchName.append("__L:_");
			matriarchName.append(std::to_string(line));
			matriarchName.append("__");

			std::string containsHint(name);
			if (10 < containsHint.size()){
				containsHint.resize(10);
				containsHint.append("..");
			}
			matriarchName.append(containsHint);

			matriarchItr.first->second.m_name = std::move(matriarchName);

			auto groupParentItr = g_trackerData.m_nodeMap.insert({ groupParent.value(), job_tracker_node() });
			if (groupParentItr.second){
				groupParentItr.first->second.m_id = groupParent;
				groupParentItr.first->second.set_node_type(job_tracker_node_matriarch);
				groupParentItr.first->second.m_name = "Unknown_Group_Parent";
				
			}
		}
	}
	return &itr.first->second;
}
job_tracker_node* job_tracker::register_batch_sub_node(constexpr_id id, const char * name)
{
	t_bufferString = name;

	if (t_bufferString.empty())
	{
		t_bufferString = "Unnamed";
	}

	const constexpr_id variation(std::hash<std::string>{}(t_bufferString));
	const constexpr_id localId(variation.merge(id));

	auto itr = g_trackerData.m_nodeMap.insert({ localId.value(), job_tracker_node() });
	if (itr.second)
	{
		itr.first->second.m_id = localId;
		itr.first->second.m_parent = id;
		itr.first->second.m_name = std::move(t_bufferString);
	}
	return &itr.first->second;
}
job_tracker_node * job_tracker::fetch_node(constexpr_id id)
{
	auto itr = g_trackerData.m_nodeMap.find(id.value());
	if (itr != g_trackerData.m_nodeMap.end())
		return &itr->second;

	return nullptr;
}
void job_tracker::dump_job_tree(const char* location)
{
	const std::string folder(location);
	const std::string programName("myprogram");
	const std::string outputFile(folder + programName + "_" + "job_graph.dgml");

	std::vector<job_tracker_node> nodes;

	for (auto node : g_trackerData.m_nodeMap){
		nodes.push_back(node.second);
	}

	std::unordered_map<std::uint64_t, std::size_t> childCounter;

	for (auto node : nodes){
		if (node.get_node_type() == job_tracker_node_default || 
			node.get_node_type() == job_tracker_node_batch){
			++childCounter[node.parent().value()];
		}
	}
	
	std::string nodesOutput;
	std::string linksOutput;

	for (auto node : nodes){
		write_node(node, childCounter, nodesOutput, linksOutput);
	}

	std::ofstream outStream;
	outStream.open(outputFile,std::ofstream::out);
	
	outStream << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
	outStream << "<DirectedGraph Title=\"DrivingTest\" Background=\"Grey\" xmlns=\"http://schemas.microsoft.com/vs/2009/dgml\">\n";

	outStream << "<Nodes>\n";
	outStream << std::move(nodesOutput);
	outStream << "</Nodes>\n";

	outStream << "<Links>\n";
	outStream << std::move(linksOutput);
	outStream << "</Links>\n";
	outStream << "</DirectedGraph>\n";

	outStream.close();
}

void write_node(const job_tracker_node& node, const std::unordered_map<std::uint64_t, std::size_t>& childCounter, std::string& outNodes, std::string& outLinks)
{
	t_bufferString.clear();

	t_bufferString.append("<Node Id=\"");
	t_bufferString.append(std::to_string(node.id().value()));
	t_bufferString.append("\"");
	t_bufferString.append(" Label=\"");
	t_bufferString.append(node.name());
	t_bufferString.append("\"");

	auto nodeType = node.get_node_type();

	if (nodeType == job_tracker_node_matriarch ||
		nodeType == job_tracker_node_batch){
		auto itr = childCounter.find(node.id().value());
		if (itr != childCounter.end() && itr->second < 30)
			t_bufferString.append(" Group=\"Expanded\"");
		else
			t_bufferString.append(" Group=\"Collapsed\"");
	}

	t_bufferString.append("  />");
	t_bufferString.append("\n");

	outNodes.append(t_bufferString);

	t_bufferString.clear();
	t_bufferString.append("<Link");

	if (nodeType == job_tracker_node_default ||
		nodeType == job_tracker_node_batch){
		t_bufferString.append(" Category=\"Contains\"");
	}

	if (node.id().value() != 0){
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
}
#endif