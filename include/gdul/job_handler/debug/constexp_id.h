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