#pragma once

#include <thread>
#include <string>

namespace gdul {

/// <summary>
/// Extension to std::thread with some extra functionality
/// </summary>
class thread : public std::thread
{
public:
	using std::thread::thread;

	/// <summary>
	/// Set thread name as for debugging
	/// </summary>
	/// <param name="name">Name</param>
	void set_name(const std::string& name);

	/// <summary>
	/// Associate thread with a core
	/// </summary>
	/// <param name="core">Desired core. Will wrap around on overflow</param>
	void set_core_affinity(std::uint8_t core);

	/// <summary>
	/// Set thread execution priority
	/// </summary>
	/// <param name="priority">Priority value</param>
	void set_execution_priority(std::int32_t priority);
	
	/// <summary>
	/// Check against thread id for validity
	/// </summary>
	/// <returns>Result</returns>
	bool valid() const noexcept;

	operator std::thread() = delete;
};
}
