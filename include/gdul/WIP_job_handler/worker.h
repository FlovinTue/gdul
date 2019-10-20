#pragma once

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {

enum class Worker_affinity_mode
{
	automatic,
	manual,
};
struct worker_info
{
	// If core affinity is set to manual, the core specified in myCoreAffinity
	// will be used, else a linear distribution between cores
	Worker_affinity_mode myCoreAffinityMode = Worker_affinity_mode::automatic;

	// If queue affinity is set to manual, the queue specified in myQueueAffinity
	// will be used, else a dynamic selection will occur from higher to lower
	// priority queues
	Worker_affinity_mode myQueueAffinityMode = Worker_affinity_mode::automatic;

	std::uint8_t myCoreAffinity;
	std::uint8_t myQueueAffinity;

	// The OS thread execution priority, as specified in WinBase.h
	std::uint32_t myExecutionPriority = 0;
};
class worker
{
public:
	worker();
	~worker();

	class core_affinity_auto;

	void init(const worker_info& info);

	void set_core_affinity_mode(Worker_affinity_mode mode);
	void set_queue_affinity_mode(Worker_affinity_mode mode);

	void set_core_affinity(std::uint8_t core);
	void set_queue_affinity(std::uint8_t queue);
	void set_execution_priority(uint32_t priority);

	void retire();

private:
	std::thread myThread;
};

}