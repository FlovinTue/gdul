#include "job_handler_tester.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include "../Common/Timer.h"
#include <gdul/delegate/delegate.h>
#include <array>
#include "../Common/util.h"
#include <gdul/job_handler/debug/job_tracker.h>

namespace gdul
{

job_handler_tester::job_handler_tester()
	: m_info()
	, m_work(1.0f)
{
}


job_handler_tester::~job_handler_tester()
{
	m_handler.retire_workers();
}

void job_handler_tester::init(const job_handler_tester_info& info)
{
	m_info = info;

	m_handler.init();

	setup_workers();
}
void job_handler_tester::basic_tests()
{
	std::vector<int> firstCollection;
	firstCollection.resize(10);
	gdul::batch_job first(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_jobQueue));
	first.enable();
	first.wait_until_finished();

	for ([[maybe_unused]] auto itr : firstCollection)
		assert(itr == 10);

	std::vector<int> secondCollection;
	secondCollection.resize(100);
	gdul::batch_job second(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_jobQueue));
	second.enable();
	second.wait_until_finished();

	assert(secondCollection.size() != 0 && secondCollection.size() != 100 && "Size should *probably* be somewhere inbetween");

	std::vector<int> thirdCollection;
	std::vector<int> thirdOutCollection;
	thirdCollection.resize(10);

	for (auto i = 0; i < thirdCollection.size(); ++i)
		thirdCollection[i] = i;

	gdul::batch_job third(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_jobQueue));
	third.enable();
	third.wait_until_finished();

	for (auto i = 0; i < thirdOutCollection.size(); ++i)
		assert(thirdOutCollection[i] == i);

	assert(thirdOutCollection.size() == thirdCollection.size());

}
void job_handler_tester::setup_workers()
{
	const std::size_t maxWorkers(std::thread::hardware_concurrency());

	std::size_t dynamicWorkers(0);
	std::size_t staticWorkers(0);
	if (m_info.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_ASSIGNED) {
		staticWorkers = maxWorkers;
	}
	if (m_info.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC) {
		dynamicWorkers = maxWorkers;
	}
	if (m_info.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_MIXED) {
		dynamicWorkers = maxWorkers / 2;
		staticWorkers = maxWorkers / 2;
	}
	
	for (std::size_t i = 0; i < dynamicWorkers; ++i) {
		worker wrk(m_handler.make_worker());
		wrk.set_core_affinity((std::uint8_t)i);
		wrk.set_execution_priority(5);
		wrk.add_assignment(&m_jobQueue);
		wrk.set_name(std::string(std::string("DynamicWorker#") + std::to_string(i + 1)));
		wrk.enable();
	}
}

float job_handler_tester::run_consumption_parallel_test(std::size_t jobs, float overDuration)
{
	auto wrap = []() {}; overDuration;
	job root(m_handler.make_job(gdul::delegate<void()>(wrap), &m_jobQueue));
	root.activate_job_tracking("consumption_parallel_root");
	job end(m_handler.make_job(gdul::delegate<void()>([this]() { std::cout << "Finished run_consumption_parallel_test." << std::endl; }), &m_jobQueue));
	end.activate_job_tracking("consumption_parallel_end");

	for (std::size_t i = 0; i < jobs; ++i)
	{
		for (std::size_t j = 0; j < m_handler.worker_count(); ++j)
		{
			job jb(m_handler.make_job(gdul::delegate<void()>(wrap), &m_jobQueue));
			jb.activate_job_tracking("consumption_parallel_intermediate");
			end.add_dependency(jb);

			jb.add_dependency(root);
			jb.enable();
		}
	}
	end.enable();

	timer<float> time;
	root.enable();
	end.wait_until_finished();

	return time.get();
}

float job_handler_tester::run_consumption_strand_parallel_test(std::size_t jobs, float overDuration)
{
	overDuration;

	auto last = [this, jobs]() {m_work.end_work(); std::cout << "Finished run_consumption_strand_parallel_test." << std::endl; };

	job root(m_handler.make_job(gdul::delegate<void()>(&work_tracker::begin_work, &m_work), &m_jobQueue));
	root.activate_job_tracking("strand_parallel_root");
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_jobQueue));
	end.add_dependency(root);
	end.activate_job_tracking("strand_parallel_end");
	end.enable();

	job next[8]{};
	next[0] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_jobQueue);
	end.add_dependency(next[0]);

	next[0].add_dependency(root);
	next[0].enable();

	std::uint8_t nextNum(1);

	for (std::size_t i = 0; i < jobs; ++i)
	{
		uint8_t children(1 + (rand() % 8));
		job intermediate[8]{};
		for (std::uint8_t j = 0; j < children; ++j, ++i)
		{
			intermediate[j] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_jobQueue);
			intermediate[j].activate_job_tracking(std::string("strand_parallel_intermediate").c_str());
			end.add_dependency(intermediate[j]);

			for (std::uint8_t dependencies = 0; dependencies < nextNum; ++dependencies)
			{
				intermediate[j].add_dependency(next[dependencies]);
			}
			intermediate[j].enable();
		}

		for (std::uint8_t j = 0; j < children; ++j)
		{
			next[j] = std::move(intermediate[j]);
		}
		nextNum = children;
	}

	timer<float> time;

	root.enable();
	end.wait_until_finished();

	return time.get();
}

float job_handler_tester::run_construction_parallel_test(std::size_t jobs, float overDuration)
{
	jobs; overDuration;

	// Hmm still something about concurrency when depending on other job during possible moving operation... Argh. Needs ASP ?
	// Reset
	// Init ( 1 threads )

	// Create root

	// 2 threads building trees
	// 6 threads building jobs depending on latest in each tree
	// Create end

	// Enable root
	// Wait for end
	return 0.0f;
}

float job_handler_tester::run_mixed_parallel_test(std::size_t jobs, float overDuration)
{
	jobs; overDuration;

	// Reset
	// Init ( max threads )

	// Create root
	// Enable root

	// 2 threads building trees
	// 6 threads building jobs depending on latest in each tree

	// Create end

	// Wait for end

	return 0.0f;
}

float job_handler_tester::run_consumption_strand_test(std::size_t jobs, float /*overDuration*/)
{
	auto last = [this]() {m_work.end_work();  std::cout << "Finished run_consumption_strand_test." << std::endl;};

	job root(m_handler.make_job((gdul::delegate<void()>(&work_tracker::begin_work, &m_work)), &m_jobQueue));
	job previous(m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_jobQueue));
	previous.add_dependency(root);
	previous.enable();

	for (std::size_t i = 0; i < jobs; ++i)
	{
		job next = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_jobQueue);

		next.add_dependency(previous);
		next.enable();

		previous = std::move(next);
	}
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_jobQueue));
	end.add_dependency(previous);
	end.enable();

	timer<float> time;

	root.enable();
	end.wait_until_finished();

	return time.get();
}

void job_handler_tester::run_scatter_test_input_output(std::size_t arraySize, std::size_t stepSize, float& outBestBatchTime, std::size_t& outBestBatchSize)
{
	arraySize; stepSize; outBestBatchSize; outBestBatchTime;
	std::uninitialized_fill(m_scatterOutput.begin(), m_scatterOutput.end(), nullptr);
	
	const std::size_t passes((arraySize / stepSize) / 5);
	
	std::array<std::pair<float, std::size_t>, 6> minTimes;
	std::uninitialized_fill(minTimes.begin(), minTimes.end(),  std::pair<float, std::size_t>(std::numeric_limits<float>::max(), 0 ));
	
	std::size_t batchSize(stepSize);
	
	timer<float> time;
	for (std::size_t i = 0; i < passes; ++i) {
	
		m_scatterInput.resize(arraySize);
	
		for (std::size_t j = 0; j < arraySize; ++j) {
			m_scatterInput[j] = (int*)(std::uintptr_t(j));
		}
	
		delegate<bool(int*&, int*&)> process(&work_tracker::scatter_process, &m_work);	
		gdul::batch_job scatter(m_handler.make_batch_job(m_scatterInput, m_scatterOutput, std::move(process), &m_jobQueue));
	
		scatter.activate_job_tracking("batch job test");

		float result(0.f);
		job endJob(m_handler.make_job([&time, &result, this]() { result = time.get(); }, &m_jobQueue));
		endJob.activate_job_tracking("batch post job");
		endJob.add_dependency(scatter);
		endJob.enable();
	
		time.reset();
		scatter.enable();
		endJob.wait_until_finished();
	
		if (result < minTimes[5].first) {
			minTimes[5] = { result, batchSize };
			std::sort(minTimes.begin(), minTimes.end(), [](std::pair<float, std::size_t>& a, std::pair<float, std::size_t>& b) {return a.first < b.first; });
		}
		const std::size_t batches(m_scatterInput.size() / batchSize + ((bool)(m_scatterInput.size() % batchSize)));
	
		batchSize += stepSize;
	}
	
	const std::size_t batches(m_scatterInput.size() / batchSize + ((bool)(m_scatterInput.size() % batchSize)));
	
	std::cout << "Completed scatter testing. Top time/batchSize are: " << minTimes[0].first << ", " << minTimes[0].second << " -------------- " << arraySize << " array size" << std::endl;
	
	outBestBatchTime = minTimes[0].first;
	outBestBatchSize = minTimes[0].second;
}

}