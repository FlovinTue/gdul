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

#include <gdul/job_handler/tracking/job_graph.h>
#include <gdul/concurrent_skip_list/concurrent_skip_list.h>
#include <gdul/job_handler/job/job.h>
#include <gdul/job_handler/job_impl.h>

#if defined (GDUL_JOB_DEBUG)
#include <Windows.h>
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

std::string executable_name() {
	char buffer[256];
	if (GetModuleFileNameA(NULL, buffer, 256)) {
		const char* cstr(buffer);

		return GDUL_FS_PATH(cstr).replace_extension().string();
	}
	return std::string("Unnamed");
}

#endif
namespace gdul {
namespace jh_detail {

#if defined (GDUL_JOB_DEBUG)
void write_dgml_node(const job_info& node, const std::unordered_map<std::uint64_t, std::size_t>& childCounter, std::string& outNodes, std::string& outLinks);
thread_local std::string t_bufferString;
#endif 

class job_graph_data
{
public:
	job_graph_data()
	{
		auto itr = m_nodeMap.insert(std::make_pair( 0, job_info() ));

#if defined(GDUL_JOB_DEBUG)
		itr.first->second.m_name = "Undeclared_Root_Node";
		itr.first->second.m_type = job_info_default;
#endif
	}
	concurrent_skip_list<std::uint64_t, job_info> m_nodeMap;
}g_trackerData;



job_info* job_graph::get_job_info(std::size_t id, const char * name, const char* file, std::uint32_t line)
{
	t_bufferString = name;

	if (t_bufferString.empty()){
		t_bufferString = "Unnamed";
	}

	const std::size_t variation(std::hash<std::string>{}(t_bufferString));

	const std::size_t groupParent(job::this_job.m_impl->get_id());
	const std::size_t groupMatriarch(id + groupParent);
	const std::size_t localId(variation + groupMatriarch);

	auto itr = g_trackerData.m_nodeMap.insert(std::make_pair(localId, job_info()));
	if (itr.second){
		itr.first->second.m_id = localId;
#if defined (GDUL_JOB_DEBUG)
		itr.first->second.m_parent = groupMatriarch;
		itr.first->second.m_name = std::move(t_bufferString);
		itr.first->second.m_physicalLocation = GDUL_FS_PATH(file).filename().string();
		itr.first->second.m_line = line;
#endif

		auto matriarchItr = g_trackerData.m_nodeMap.insert(std::make_pair(groupMatriarch, job_info()));
		if (matriarchItr.second){
			matriarchItr.first->second.m_id = groupMatriarch;
			matriarchItr.first->second.m_parent = groupParent;
			matriarchItr.first->second.set_node_type(job_info_matriarch);
			matriarchItr.first->second.m_physicalLocation = GDUL_FS_PATH(file).filename().string();
			matriarchItr.first->second.m_line = line;
			std::string matriarchName;
			matriarchName.append(matriarchItr.first->second.m_physicalLocation);
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

			auto groupParentItr = g_trackerData.m_nodeMap.insert(std::make_pair(groupParent, job_info()));
			if (groupParentItr.second){
				groupParentItr.first->second.m_id = groupParent;
				groupParentItr.first->second.set_node_type(job_info_matriarch);
				groupParentItr.first->second.m_name = "Unknown_Group_Parent";
			}
		}
	}
	return &itr.first->second;
}
job_info* job_graph::get_job_info_sub(std::size_t id, const char * name)
{
	t_bufferString = name;

	if (t_bufferString.empty())
	{
		t_bufferString = "Unnamed";
	}

	const std::size_t variation(std::hash<std::string>{}(t_bufferString));
	const std::size_t localId(variation + id);

	auto itr = g_trackerData.m_nodeMap.insert(std::make_pair(localId, job_info()));
	if (itr.second)
	{
		auto parent = g_trackerData.m_nodeMap.find(id);

		itr.first->second.m_id = localId;
		itr.first->second.m_parent = id;
		itr.first->second.m_name = std::move(t_bufferString);
		itr.first->second.m_physicalLocation = parent->second.physical_location();
		itr.first->second.m_line = parent->second.line();
	}
	return &itr.first->second;
}
job_info * job_graph::fetch_node(std::size_t id)
{
	auto itr = g_trackerData.m_nodeMap.find(id);
	if (itr != g_trackerData.m_nodeMap.end())
		return &itr->second;

	return nullptr;
}
#if defined (GDUL_JOB_DEBUG)
void job_graph::dump_job_tree(const char* location)
{
	const std::string folder(location);
	const std::string programName(executable_name());
	const std::string outputFile(folder + programName + "_" + "job_graph.dgml");

	std::vector<job_info> nodes;

	for (auto& node : g_trackerData.m_nodeMap){
		nodes.push_back(node.second);
	}

	std::unordered_map<std::uint64_t, std::size_t> childCounter;

	for (auto& node : nodes){
		if (node.get_node_type() == job_info_default || 
			node.get_node_type() == job_info_batch){
			++childCounter[node.parent()];
		}
	}
	
	std::string nodesOutput;
	std::string linksOutput;

	for (auto& node : nodes){
		write_dgml_node(node, childCounter, nodesOutput, linksOutput);
	}

	std::ofstream outStream;
	outStream.open(outputFile,std::ofstream::out);
	
	outStream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
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

void write_dgml_node(const job_info& node, const std::unordered_map<std::uint64_t, std::size_t>& childCounter, std::string& outNodes, std::string& outLinks)
{
	t_bufferString.clear();

	t_bufferString.append("<Node Id=\"");
	t_bufferString.append(std::to_string(node.id()));
	t_bufferString.append("\"");
	t_bufferString.append(" Label=\"");
	t_bufferString.append(node.name());
	t_bufferString.append("\"");

	auto nodeType = node.get_node_type();

	if (nodeType == job_info_matriarch ||
		nodeType == job_info_batch){
		auto itr = childCounter.find(node.id());
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

	if (nodeType == job_info_default ||
		nodeType == job_info_batch){
		t_bufferString.append(" Category=\"Contains\"");
	}

	if (node.id() != 0){
		t_bufferString.append(" Source=\"");
		t_bufferString.append(std::to_string(node.parent()));
		t_bufferString.append("\" Target = \"");
		t_bufferString.append(std::to_string(node.id()));
		t_bufferString.append("\"");

		t_bufferString.append("  />");
		t_bufferString.append("\n");

		outLinks.append(t_bufferString);
	}
}
void write_job_time_set(const time_set& timeSet, std::ofstream& toStream, const char* withName)
{
	toStream << "<time_set name=\"" << withName << "\">\n";
	toStream << "<avg_time>" << timeSet.get_avg() << "</avg_time>\n";
	toStream << "<min_time>" << timeSet.get_min() << "</min_time>\n";
	toStream << "<max_time>" << timeSet.get_max() << "</max_time>\n";
	toStream << "<min_timepoint>" << timeSet.get_minTimepoint() << "</min_timepoint>\n";
	toStream << "<max_timepoint>" << timeSet.get_maxTimepoint() << "</max_timepoint>\n";
	toStream << "<completion_count>" << timeSet.get_completion_count() << "</completion_count>\n";
	toStream << "</time_set>\n";
}
void job_graph::dump_job_time_sets(const char* location)
{
	const std::string folder(location);
	const std::string programName(executable_name());
	const std::string outputFile(folder + programName + "_" + "job_time_sets.xml");

	std::ofstream outStream;
	outStream.open(outputFile, std::ofstream::out);

	outStream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	outStream << "<jobs>\n";
	
	for (auto& itr : g_trackerData.m_nodeMap) {
		if (!itr.second.m_completionTimeSet.get_completion_count() &&
			!itr.second.m_enqueueTimeSet.get_completion_count() &&
			!itr.second.m_waitTimeSet.get_completion_count())
			continue;

		outStream << "<job id=\"";
		outStream << itr.second.id();
		outStream << "\">\n";

		outStream << "<job_name>" << itr.second.name() << "</job_name>\n";
		outStream << "<physical_job>" << itr.second.physical_location() + "__L:_" + std::to_string(itr.second.line()) << "</physical_job>\n";
		
		if (itr.second.m_completionTimeSet.get_completion_count())
			write_job_time_set(itr.second.m_completionTimeSet, outStream, "completion_time");
		if (itr.second.m_enqueueTimeSet.get_completion_count())
			write_job_time_set(itr.second.m_enqueueTimeSet, outStream, "enqueue_time");
		if (itr.second.m_waitTimeSet.get_completion_count())
			write_job_time_set(itr.second.m_waitTimeSet, outStream, "wait_time");

		outStream << "</job>\n";
	}

	outStream << "</jobs>\n";

	outStream.close();
}
}
}
#endif