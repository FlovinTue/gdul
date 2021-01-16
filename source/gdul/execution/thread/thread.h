#pragma once

#include <thread>
#include <string>
#include <gdul/delegate/delegate.h>

namespace gdul {
class thread : public std::thread
{
public:
	using std::thread::thread;

	void set_name(const std::string& name);
	void set_core_affinity(std::uint8_t core);
	void set_execution_priority(std::int32_t priority);
	
	bool valid() const noexcept;

	operator std::thread() = delete;
};
}
