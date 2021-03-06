#include "job_handler_tester.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include "../Common/Timer.h"
#include <gdul/utility/delegate.h>
#include <array>
#include "../Common/util.h"
#include <gdul/execution/job_handler/tracking/job_graph.h>
#include <gdul/execution/thread/thread.h>

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
	gdul::job a(m_handler.make_job([]() {}, &m_syncQueue));
	gdul::job b(m_handler.make_job([]() {}, &m_syncQueue, "name"));
	gdul::job c(m_handler.make_job([]() {}, &m_syncQueue, 1));
	gdul::job d(m_handler.make_job([]() {}, &m_syncQueue, 2, "name2"));

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
	gdul::batch_job first1(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_syncQueue));
	first1.enable();
	first1.wait_until_finished();
	gdul::batch_job first2(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_syncQueue, "first2"));
	first2.enable();
	first2.wait_until_finished();
	gdul::batch_job first3(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_syncQueue, 3));
	first3.enable();
	first3.wait_until_finished();
	gdul::batch_job first4(m_handler.make_batch_job(firstCollection, gdul::delegate<void(int&)>([](int& elem) {elem = 10; }), &m_syncQueue, 4, "first4"));
	first4.enable();
	first4.wait_until_finished();

	for ([[maybe_unused]] auto itr : firstCollection)
		assert(itr == 10);

	std::vector<int> secondCollection;
	secondCollection.resize(100);
	gdul::batch_job second1(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_syncQueue));
	second1.enable();
	second1.wait_until_finished();
	gdul::batch_job second2(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_syncQueue, "second2"));
	second2.enable();
	second2.wait_until_finished();
	gdul::batch_job second3(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_syncQueue, 3));
	second3.enable();
	second3.wait_until_finished();
	gdul::batch_job second4(m_handler.make_batch_job(secondCollection, gdul::delegate<bool(int&)>([](int& elem) {elem = 10; return rand() % 2; }), &m_syncQueue, 4, "second4"));
	second4.enable();
	second4.wait_until_finished();

	assert(secondCollection.size() != 0 && secondCollection.size() != 100 && "Size should *probably* be somewhere inbetween");

	std::vector<int> thirdCollection;
	std::vector<int> thirdOutCollection;
	thirdCollection.resize(10);

	for (auto i = 0; i < thirdCollection.size(); ++i)
		thirdCollection[i] = i;

	gdul::batch_job third1(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_syncQueue));
	third1.enable();
	third1.wait_until_finished();
	gdul::batch_job third2(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_syncQueue, "third2"));
	third2.enable();
	third2.wait_until_finished();
	gdul::batch_job third3(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_syncQueue, 3));
	third3.enable();
	third3.wait_until_finished();
	gdul::batch_job third4(m_handler.make_batch_job(thirdCollection, thirdOutCollection, gdul::delegate<bool(int&, int&)>([](int& in, int& out) { out = in; return true; }), &m_syncQueue, 4, "third4"));
	third4.enable();
	third4.wait_until_finished();

	for (auto i = 0; i < thirdOutCollection.size(); ++i)
		assert(thirdOutCollection[i] == i);

	assert(thirdOutCollection.size() == thirdCollection.size());

}
void job_handler_tester::setup_workers()
{
	const std::size_t maxWorkers(std::thread::hardware_concurrency());

	for (std::size_t i = 0; i < maxWorkers; ++i) {
		worker wrk(m_handler.make_worker());
		wrk.get_thread()->set_core_affinity((std::uint8_t)i);
		wrk.get_thread()->set_execution_priority(5);
		//wrk.add_assignment(&m_syncQueue);
		wrk.add_assignment(&m_syncQueue);
		wrk.get_thread()->set_name(std::string(std::string("DynamicWorker#") + std::to_string(i + 1)));
		wrk.enable();
	}
}

float job_handler_tester::run_consumption_parallel_test(std::size_t jobs, float overDuration)
{
	auto wrap = []() {}; overDuration;
	job root(m_handler.make_job(gdul::delegate<void()>(wrap), &m_syncQueue, "consumption_parallel_root"));

	job end(m_handler.make_job(gdul::delegate<void()>([this]() { std::cout << "Finished run_consumption_parallel_test." << std::endl; }), &m_syncQueue, "consumption_parallel_end"));

	for (std::size_t i = 0; i < jobs; ++i)
	{
		for (std::size_t j = 0; j < m_handler.worker_count(); ++j)
		{
			job jb(m_handler.make_job(gdul::delegate<void()>(wrap), &m_syncQueue, "consumption_parallel_intermediate"));
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

	job root(m_handler.make_job(gdul::delegate<void()>(&work_tracker::begin_work, &m_work), &m_syncQueue, "strand_parallel_root"));
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_syncQueue, "strand_parallel_end"));
	end.depends_on(root);
	end.enable();

	job next[8]{};
	next[0] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_syncQueue);
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
			intermediate[j] = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_syncQueue, "strand_parallel_intermediate");

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

	job root(m_handler.make_job((gdul::delegate<void()>(&work_tracker::begin_work, &m_work)), &m_syncQueue));
	job previous(m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_syncQueue));
	previous.depends_on(root);
	previous.enable();

	for (std::size_t i = 0; i < jobs; ++i)
	{
		job next = m_handler.make_job(gdul::delegate<void()>(&work_tracker::main_work, &m_work), &m_syncQueue);

		next.depends_on(previous);
		next.enable();

		previous = std::move(next);
	}
	job end(m_handler.make_job(gdul::delegate<void()>(last), &m_syncQueue));
	end.depends_on(previous);
	end.enable();

	timer<float> time;

	root.enable();
	end.wait_until_finished();

	return time.get();
}

float job_handler_tester::run_predictive_scheduling_test()
{
	job root(m_handler.make_job([]() {}, &m_syncQueue, "Predictive Scheduling Root"));
	job dependant(m_handler.make_job([]() {}, &m_syncQueue, "Predictive Scheduling End"));

	auto spinFor = [](double ms) {
		timer<double> t;
		const double sec(0.001 * ms);

		float accum((float)ms);
		while (t.get() < sec)
			accum = std::sqrtf(accum);
	};

	// The optimal execution should be parallelSplit * serialExecutionTime,
	// However, since the forked jobs are added first, in a normal scenario
	// the execution time should end up being (parallelSplit * serialexecutionTime) + serialExecutionTime
	// As read now, that would be 8 * 1.0 ms vs (8 * 1.0 ms) + 1.0 ms

	const double serialExecutionTime(1.0);
	const std::size_t parallelSplit(std::thread::hardware_concurrency());


	float parallelPriority(0.f);
	float serialPriority(0.f);

	timer<float> time;

	for (std::size_t i = 0; i < parallelSplit; ++i) {
		job jb(m_handler.make_job(make_delegate(spinFor, serialExecutionTime), &m_syncQueue, i, "Predictive Scheduling Parallel"));
		//jb.depends_on(root);
		jb.enable();
		dependant.depends_on(jb);

		if (i == 0)
			parallelPriority = jb.priority();
	}

	job previous;
	for (std::size_t i = 0; i < parallelSplit; ++i) {
		job jb(m_handler.make_job(make_delegate(spinFor, serialExecutionTime), &m_syncQueue, i, std::string("Predictive Scheduling Serial# " + std::to_string(i)).c_str()));
		jb.depends_on(previous);
		//jb.depends_on(root);
		dependant.depends_on(jb);

		if (i == 0)
			serialPriority = jb.priority();

		previous = std::move(jb);
		previous.enable();
	}

	dependant.enable();
	root.enable();
	dependant.wait_until_finished();

	const float result = time.get();
	//
	//std::cout << "Finished predictive scheduling test with " << result * 1000.f << " ms execution time using " << std::endl;
	//std::cout << "...The optimal execution time should be " << (double)parallelSplit * serialExecutionTime << " ms" << std::endl;
	//std::cout << "...Suboptimal execution time should be " << (double)parallelSplit * serialExecutionTime + serialExecutionTime << " ms" << std::endl;
	//std::cout << "ParallelPrio " << parallelPriority << " SerialPrio: " << serialPriority << std::endl;

	return result;
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
		gdul::batch_job scatter(m_handler.make_batch_job(m_scatterInput, m_scatterOutput, std::move(process), &m_syncQueue));
	
		float result(0.f);
		job endJob(m_handler.make_job([&time, &result, this]() { result = time.get(); }, &m_syncQueue, "batch post job"));
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