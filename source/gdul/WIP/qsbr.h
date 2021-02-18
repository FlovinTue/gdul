#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace gdul::qsbr {

constexpr std::uint8_t MaxThreads = 64;

using access_state = std::atomic<std::size_t>;

class critical_section
{
public:
	critical_section();
	~critical_section();
};

void register_thread();
void unregister_thread();

/// <summary>
/// Reset item to 'unsafe' state
/// </summary>
/// <param name="item">Access state item</param>
/// 
void reset_state(access_state& item);
bool initialize_state(access_state& item);
bool update_state(access_state& item);
bool check_state(const access_state& item);

}