#pragma once

#include <cstdint>
#include <cstddef>

// Define to enable some debug features
#ifdef _DEBUG
#define GDUL_DEBUG
#endif

namespace gdul
{

enum job_queue : std::uint8_t
{
	job_queue_1,
	job_queue_2,
	job_queue_3,

	// Leave in place
	job_queue_count,
};

namespace jh_detail
{
constexpr job_queue Default_Job_Queue = job_queue(0);

constexpr std::uint16_t Job_Handler_Max_Workers = 32;

// Batch job will clamp batchSize so that this value is not exceeded
constexpr std::uint16_t Batch_Job_Max_Batches = 128;

// The number of chunks allocated per chunk block
constexpr std::size_t Job_Impl_Allocator_Block_Size = 1024;
// The number of chunks allocated per chunk block
constexpr std::size_t Batch_Job_Allocator_Block_Size = 8;
}
}