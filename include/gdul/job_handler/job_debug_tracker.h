#pragma once

#include <gdul/job_handler/globals.h>

#if defined (GDUL_JOB_DEBUG)

#include <type_traits>

#define GDUL_CEXPR_ID_COUNTER __COU##NTER__
#define GDUL_CEXPR_ID() gdul::constexpr_id::make<GDUL_CEXPR_ID_COUNTER>()

namespace gdul {
class job;
class batch_job;

struct constexpr_id
{
template <std::uint64_t Id>
static constexpr constexpr_id make() {
	return constexpr_id(Id);
}

std::uint64_t value() const noexcept {return m_val;}

private:
	std::uint64_t m_val;

	constexpr constexpr_id(std::uint64_t id)
		: m_val(id)
	{}
};

namespace jh_detail {


class job_debug_tracker
{
public:
	static void register_node(constexpr_id id);
	static void add_node_variation(constexpr_id id, const char* name);

	static void dump_job_tree(const char* location);

};
}
}
#endif