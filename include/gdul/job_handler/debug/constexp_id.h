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

#pragma once

#include <gdul/job_handler/globals.h>

#if defined(GDUL_JOB_DEBUG)
namespace gdul
{
namespace jh_detail
{
class job_tracker;
}
struct constexpr_id
{
	template <std::uint64_t Id>
	static constexpr constexpr_id make()
	{
		return constexpr_id(Id);
	}

	constexpr_id merge(const constexpr_id& other) const
	{
		return constexpr_id(m_val + other.m_val);
	}
	std::uint64_t value() const noexcept { return m_val; }

	constexpr_id(const constexpr_id&) = default;
	constexpr_id(constexpr_id&&) = default;
	constexpr_id& operator=(const constexpr_id&) = default;
	constexpr_id& operator=(constexpr_id&&) = default;

private:
	friend class jh_detail::job_tracker;

	std::uint64_t m_val;

	constexpr constexpr_id(std::uint64_t id)
		: m_val(id)
	{}
};
}


#endif