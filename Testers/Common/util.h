#pragma once

#define GDUL_ASSERT(cond) if (!cond) {size_t* null(nullptr); gdul::debugCollect += *null;}
#define GDUL_BREAK_CATCHER volatile size_t _gdul_dbg_var = 1; gdul::debugCollect += _gdul_dbg_var;

namespace gdul
{
static size_t debugCollect = 0;

}