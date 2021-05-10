// Copyright(c) 2021 Flovin Michaelsen
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

#pragma once

#include <thread>
#include <string>

namespace std::this_thread {
void set_name(const std::string& name);
void set_core_affinity(std::uint8_t core);
void set_execution_priority(std::int32_t core);

bool valid() noexcept;

std::thread::native_handle_type native_handle();
}

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

private:
	friend void std::this_thread::set_name(const std::string&);
	friend void std::this_thread::set_core_affinity(std::uint8_t);
	friend void std::this_thread::set_execution_priority(std::int32_t);

	static void set_name(const std::string& name, std::thread::native_handle_type handle);
	static void set_core_affinity(std::uint8_t core, std::thread::native_handle_type handle);
	static void set_execution_priority(std::int32_t priority, std::thread::native_handle_type handle);
};
}