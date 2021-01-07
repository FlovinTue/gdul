#include "job_handler_tester.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include "../Common/Timer.h"
#include <gdul/delegate/delegate.h>
#include <array>
#include "../Common/util.h"
#include <gdul/job_handler/tracking/job_graph.h>

namespace gdul
{

job_handler_tester::job_handler_tester()
	: m_info()
	, m_work(1.0f)
{
}


job_handler_tester::~job_handler_tester()
{
	m_handler.shutdown();
}

void job_handler_tester::init(const job_handler_tester_info& info)
{
	m_info = info;

	m_handler.init();

	setup_workers();
}

void job_handler_tester::basic_tests()
{
	gdul::job a(m_handler.make_job([]() {}, &m_asyncQueue));
	gdul::job b(m_handler.make_job([]() {}, &m_asyncQueue, "name"));
	gdul::job c(m_handler.make_job([]() {}, &m_asyncQueue, 1));
	gdul::job d(m_handler.make_job([]() {}, &m_asyncQueue, 2, "name2"));

	a.enable();
	a.wait_until_finished();
	b.enable();
	b.wait_until_finished();
	c.enable();
	c.wait_until_finished();
	d.enable();
	d.wait_until_finished();

	std::vector<int> firstCollection;
	firstCollection.resize(10);
	gdul::batch_job first1(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_asyncQueue));
	first1.enable();
	first1.wait_until_finished();
	gdul::batch_job first2(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_asyncQueue, "first2"));
	first2.enable();
	first2.wait_until_finished();
	gdul::batch_job first3(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_asyncQueue, 3));
	first3.enable();
	first3.wait_until_finished();
	gdul::batch_job first4(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_asyncQueue, 4, "first4"));
	first4.enable();
	first4.wait_until_finished();

	for ([[maybe_unused]] auto itr : firstCollection)
		assert(itr == 10);

	std::vector<int> secondCollection;
	secondCollection.resize(100);
	gdul::batch_job second1(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_asyncQueue));
	second1.enable();
	second1.wait_until_finished();
	gdul::batch_job second2(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_asyncQueue, "second2"));
	second2.enable();
	second2.wait_until_finished();
	gdul::batch_job second3(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_asyncQueue, 3));
	second3.enable();
	second3.wait_until_finished();
	gdul::batch_job second4(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_asyncQueue, 4, "second4"));
	second4.enable();
	second4.wait_until_finished();

	assert(secondCollection.size() != 0 && secondCollection.size() != 100 && "Size should *probably* be somewhere inbetween");

	std::vector<int> thirdCollection;
	std::vector<int> thirdOutCollection;
	thirdCollection.resize(10);

	for (auto i = 0; i < thirdCollection.size(); ++i)
		thirdCollection[i] = i;

	gdul::batch_job third1(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_asyncQueue));
	third1.enable();
	third1.wait_until_finished();
	gdul::batch_job third2(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_asyncQueue, "third2"));
	third2.enable();
	third2.wait_until_finished();
	gdul::batch_job third3(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_asyncQueue, 3));
	third3.enable();
	third3.wait_until_finished();
	gdul::batch_job third4(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_asyncQueue, 4, "third4"));
	third4.enable();
	third4.wait_until_finished();

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
		wrk.add_assignment(&m_asyncQueue);
		wrk.set_name(std::string(std::string("DynamicWorker#") + std::to_string(i + 1)));
		wrk.enable();
	}
}

float job_handler_tester::run_consumption_parallel_test(std::size_t jobs, float overDuration)
{
	auto wrap = []() {}; overDuration;
	job root(m_handler.make_job(gdul::delegate<void()>(wrap), &m_asyncQueue, "consumption_parallel_root"));

	job end(m_handler.make_job(gdul::delegate<void()>([this]() { std::cout << "Finished run_consumption_parallel_test." << std::endl; }), &m_asyncQueue, "consumption_parallel_end"));

	for (std::size_t i = 0; i < jobs; ++i)
	{
		for (std::size_t j = 0; j < m_handler.worker_count(); ++j)
		{
			job jb(m_handler.make_job(gdul::delegate<void()>(wrap), &m_asyncQueue, "consumption_parallel_intermediate"));
			end.depends_on(jb);

			jb.depends_on(root);
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

	job root(m_handler.make_job(gdul::delegate<void()>(&work_tracker::begin_work, &m_work), &m_asyncQueue, "strand_parallel_root"));
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_asyncQueue, "strand_parallel_end"));
	end.depends_on(root);
	end.enable();

	job next[8]{};
	next[0] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_asyncQueue);
	end.depends_on(next[0]);

	next[0].depends_on(root);
	next[0].enable();

	std::uint8_t nextNum(1);

	for (std::size_t i = 0; i < jobs; ++i)
	{
		uint8_t children(1 + (rand() % 8));
		job intermediate[8]{};
		for (std::uint8_t j = 0; j < children; ++j, ++i)
		{
			intermediate[j] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_asyncQueue, "strand_parallel_intermediate");

			end.depends_on(intermediate[j]);

			for (std::uint8_t dependencies = 0; dependencies < nextNum; ++dependencies)
			{
				intermediate[j].depends_on(next[dependencies]);
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

float job_handler_tester::run_consumption_strand_test(std::size_t jobs, float /*overDuration*/)
{
	auto last = [this]() {m_work.end_work();  std::cout << "Finished run_consumption_strand_test." << std::endl;};

	job root(m_handler.make_job((gdul::delegate<void()>(&work_tracker::begin_work, &m_work)), &m_asyncQueue));
	job previous(m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_asyncQueue));
	previous.depends_on(root);
	previous.enable();

	for (std::size_t i = 0; i < jobs; ++i)
	{
		job next = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_asyncQueue);

		next.depends_on(previous);
		next.enable();

		previous = std::move(next);
	}
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_asyncQueue));
	end.depends_on(previous);
	end.enable();

	timer<float> time;

	root.enable();
	end.wait_until_finished();

	return time.get();
}

void job_handler_tester::run_predictive_scheduling_test()
{
}

void job_handler_tester::run_scatter_test_input_output(std::size_t arraySize, std::size_t stepSize, float& outBestBatchTime)
{
	arraySize; stepSize; outBestBatchTime;
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
		gdul::batch_job scatter(m_handler.make_batch_job(m_scatterInput, m_scatterOutput, std::move(process), &m_asyncQueue));
	
		float result(0.f);
		job endJob(m_handler.make_job([&time, &result, this]() { result = time.get(); }, &m_asyncQueue, "batch post job"));
		endJob.depends_on(scatter);
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
	
	std::cout << "Completed scatter testing. Top time: " << minTimes[0].first << ", " << minTimes[0].second << " -------------- " << arraySize << " array size" << std::endl;
	
	outBestBatchTime = minTimes[0].first;
}

}