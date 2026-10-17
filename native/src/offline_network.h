#pragma once
#include <cstdint>
namespace fx::offline_network {
// Process-local guard. It is installed before the game entrypoint executes and
// remains in place until process exit; it changes no firewall/Steam settings.
void install();
uint64_t blocked_calls() noexcept;
}
