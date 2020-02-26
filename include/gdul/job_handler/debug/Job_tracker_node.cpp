#include "Job_tracker_node.h"

namespace gdul
{
namespace jh_detail
{
constexpr_id job_tracker_node::id() const
{
	return m_id;
}

constexpr_id job_tracker_node::parent() const
{
	return m_parent;
}

}
}
