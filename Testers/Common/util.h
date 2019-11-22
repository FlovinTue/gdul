#pragma once

#define GDUL_ASSERT(cond) if (!cond) {size_t* null(nullptr); gdul::assertCollect += *null;}

namespace gdul
{
static size_t assertCollect = 0;

}