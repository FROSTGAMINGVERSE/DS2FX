# Training Stamina 0.1.0

Optional DS2FX Core 0.3.0-alpha.1 / API 2 module. Offline testing only.
Installed default: **observe**. This installs the version-checked call-site hook but
passes costs through unchanged. It is not an infinite-stamina cheat.

In `Root/Game/DS2FX/Plugins/frost-training-stamina/training.ini`, `Mode=training`
enables `CostPercent` (10..100, default 50). Core scales only finite positive costs
with reason code 0 for the current living local player. All other arguments,
regeneration and other characters pass through. The original engine routine still
performs its checks, writes and side effects. Live traces support roughly half continuous/sprint drain; recorded discrete action
costs are unchanged. Do not advertise universal half-cost behavior.

First test: keep observe, launch offline through MO2, load a character, attack/roll,
then exit normally. Check the runtime log for `local>0`, `eligible>0`, `modified=0`,
`thread_fault=0` and a hook thread different from `observer_thread`. After this
check, change Mode to training with game closed and compare the same action from
full stamina. Restore observe or disable this mod to remove reduced costs next launch.

One policy owner is allowed; competing policy modules are refused at initialization.
A second observed local-player caller thread permanently disables scaling for that
process. Initialization, byte mismatch, or hook backend failures abort launch.
No general game-thread scheduler or safe arbitrary engine-call API is provided.
Native modules remain trusted code. No Seamless/multiplayer support.

Validation: original code-site disassembly and full 421-byte hash; actual MinHook/
MASM argument/return preservation and reduction in an owned synthetic host. Live session36588 records1,093 modified calls without a thread fault and roughly
halved continuous drain, with clean exit/restoration. See SDK/VALIDATION.md.
