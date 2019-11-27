#pragma once

namespace gdul {
namespace jh_detail {
class callable_base
{
public:
	virtual ~callable_base() = default;

	virtual void operator()() = 0;
};
}
}
