#pragma once

#include <cstddef>


namespace gdul {

namespace ebr {


using ebr_item = std::size_t;


void register_thread();
void unregister_thread();

bool unreferenced(ebr_item item);

void enter_cs();
void exit_cs();

}
}