# Player state service — Core 0.5.26-alpha.1 development candidate

ABI 2 now has four appended capabilities: register a player state, request a
transition, query the service, and register idle/moving/sprint jump routes.
The original 96-byte API prefix and existing structures are unchanged. New
modules must check the API size before reading the new function pointers.

This is a working registration/transition service with an owned-fixture
executor. It is **not yet an in-game extra-state implementation**: this build
does not bind a native jump executor, allocate a new Morpheme node, redirect
animation dispatch, or change jump physics. Production reports
`executor_available=0`; custom execution requests cannot become active.

## Module contract

`ds2fx_player_states.h` contains pointer-free definitions, requests, route
handles, status and result codes. See `examples/jump_states.cpp` for a complete
module. Register during the Initialize call on its actual initialization
thread. The runtime authenticates the calling DLL; registrations commit only
after its manifest identity is validated. A callback cannot register more
states. Handles are process-local and belong to that DLL; another DLL cannot
request them, even if it learns the handle value. Native modules remain trusted
code, not sandboxed.

Definitions use owner-local IDs and at most 32 states across all owners. The
initial supported motion intents are native pass-through, a grounded animation
action, vertical jump and forward jump. Animation IDs are asset references,
not authority to call an engine routine. A Core executor must separately
validate the actual installed content and its native graph route.

One canonical local action can be queued/active at a time. A request has the
observed character epoch and a 1..250ms expiry. `OK` means queued; only a native
execution receipt makes it active/completed. A missing completion receipt
expires at the definition's 100..10000ms watchdog. The watchdog revokes DS2FX
tracking authority; it does not invent a game movement stop or rollback.

Status queries copy a snapshot. `character_epoch` is the current observed
generation, not persistent character/save identity. Sequence changes identify
state-service transitions; they are not game frame numbers. Query the result
and phase rather than treating a handle as proof of execution.

## First route: three-way jumping

The service routes a known jump press edge on a grounded, canonical, living
player in a known playable context. The route is chosen once at takeoff:

| Input state | Route |
| --- | --- |
| Idle | Vertical jump state |
| Moving without sprint | Forward jump state |
| Sprint | Native pass-through; no custom executor call |

Holding jump cannot repeat it, including after landing. Unknown input/context,
travel or character replacement requires a known release before another press.
Movement changes while active cannot swap the takeoff route. Air steering and
double jumping are outside this first service contract.

The example's IDs 947/948 are existing test asset references. Installing the
previous animation-only replacement archive would also change the shared
sprint animation. The example DLL and those archives are not installed or
enabled by this upgrade.

## Core adapter boundary

`src/player_states.h` exposes the internal `NativeAdapter` interface. It is
not exported through the plugin ABI. Core binds it before workers/hooks start,
and keeps its lifetime through process exit. `sample` must authenticate the
canonical local player, generation, actual game thread, native input, scene,
grounding and contextual exclusions. Unknown fields must remain unknown.

`start` must own the real animation/physics dispatch transaction and preserve
the original action on rejection. A later sidecar animation call is not a
replacement for intercepting native takeoff. `poll` must return actual native
completion/refusal receipts, with independent ownership checks for every
engine operation. An observation callback may request/revoke; it never executes.

The existing exact-hash GameManUpdate seam calls `native_tick` when its guarded
Lantern hook is installed. With no adapter bound, that call returns immediately.
A future player-state adapter must install/authenticate its own required tick
route independently of whether Lantern is enabled, and prove that the chosen
takeoff interception executes at the right point relative to native dispatch.
The Lantern seam alone does not establish a jump executor or input-consumption
contract. Native ticks use try-lock; contention skips a frame rather than
blocking on plugin work. Storage is fixed and no plugin callbacks run there.

Both offline Core and Seamless bridge compile the service/API. The bridge's
existing module filter is retained: this new gameplay example is not silently
allowed online. Native state actions are network-visible gameplay and require
a separately implemented DS2FX bridge adapter and peer synchronization.
No two-peer, join/leave, travel, death or character-change certification is
claimed by fixtures or by successful builds. Seamless itself is unchanged.

## Validation and installation

Run `player_states` with CTest, and `tests/player-states-integration.ps1` for
the actual runtime DLL/calling-module ownership fixture. Existing runtime and
Seamless integration scripts check loading, original ABI consumers, filtering,
authentication and shutdown. Fixture logs explicitly identify synthetic hosts.

The source/package version is 0.5.26-alpha.1, module contract remains
0.3.0-alpha.1 and ABI remains 2. This is a local development candidate. Installed
Core/mod/content was not replaced. Source backups and staged binary hashes are
under `local/player-state-upgrade`. Root Builder still owns Build/Sync/Clear
for any later deployment. No proprietary animation or game archives are
included in the state-service candidate.
