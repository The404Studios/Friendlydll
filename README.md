# Friendlydll

A feature-rich internal DLL for Garry's Mod, built with C++17. Hooks into the Source Engine via DirectX 9 and provides a full ImGui overlay with 60+ modules spanning combat, movement, ESP, DarkRP automation, autonomous bot AI, voice intelligence, Lua execution, and layered anti-detection.

---

## Architecture

```
Friendlydll.dll
  |
  +-- DllMain (dllmain.cpp)
  |     Worker thread -> Interface capture -> Netvar dump -> Hook init
  |     -> Lua state grab -> Scheduler -> ConVar loop -> Eject poll
  |
  +-- D3D9 Hook (d3d9_hook.cpp)
  |     Kiero -> IDirect3DDevice9::Present -> WndProc subclass -> ImGui
  |
  +-- Core Hooks (core/hooks.cpp)
  |     CreateMove, DrawModelExecute, FrameStageNotify
  |
  +-- Features (core/*.hpp)
  |     62 header-only modules, double-buffered data, atomic state
  |
  +-- Lua Engine (core/lua_loader.cpp + lua_scripts.hpp)
  |     Runtime CLuaShared execution, ~320 KB embedded scripts
  |
  +-- Intel System (core/intel_lua.hpp)
  |     Per-frame Lua query -> player data + entity scan -> double-buffered
  |
  +-- Bot AI (core/bot_*.hpp + follow_bot.hpp)
  |     Threat scanner -> Engagement rules -> Nav graph (A*) -> Task FSM
  |
  +-- Scheduler (core/scheduler.cpp)
        Async periodic tasks (ConVar enforcement, fullbright, etc.)
```

## Features

### Combat

| Module | Description |
|--------|-------------|
| **Aimbot** | Silent aim, smooth interpolation, configurable FOV cone, per-bone selection (head/chest/pelvis), priority weighting (FOV 60% + distance 30% + health 20% + threat 50%), target lock for N ticks, humanizer mode |
| **Triggerbot** | Multi-point trace (center + 4 offsets for hitbox coverage), configurable delay (ms), head-only filter, hold-key or always-on, per-player whitelist/blacklist |
| **Backtrack** | 12-tick ring buffer per player, multi-bone scan (head/chest/pelvis), composite scoring (FOV 60% + freshness 25% + visibility 15%), 200ms server window enforcement |
| **Resolver** | Velocity-based anti-aim detection, jitter detection (180-degree oscillation), adaptive brute-force across 8 yaw offsets (0/180/90/-90/45/-45/135/-135), per-offset hit/miss tracking with confidence scoring, pelvis-to-head model yaw extraction |
| **Prediction** | Per-player velocity + acceleration tracking (EMA smoothed), ghost skeleton rendering at predicted future position with dashed leader lines |
| **Recoil Control** | View punch removal (2x compensation) |

### Tick Manipulation

| Module | Description |
|--------|-------------|
| **Lag Switch** | Hold-key packet choking (Mouse4 default), configurable duration, burst release, pulsing HUD indicator with charge bar |
| **Doubletap** | Tickbase shift (14 ticks), charge-then-fire mechanic (Mouse5 default), instant double-shot on release |
| **Speedhack** | Extra command generation per tick, configurable multiplier, movement speed scaling |

### Anti-Aim

| Mode | Description |
|------|-------------|
| **Jitter** | Alternating yaw offset (configurable range, default 120 degrees) |
| **Spin** | Continuous yaw rotation at configurable speed |
| **Backward** | +180 degree yaw flip |
| **Random** | Uniform random yaw each tick |
| **Desync** | Body yaw offset with periodic side-flip + micro-movement to control LBY updates |
| **Micro** | Small random offsets around backward to confuse resolvers |
| **Pitch modes** | Down (89), up (-89), jitter pitch, zero |

### Fake Lag

| Mode | Description |
|------|-------------|
| **Static** | Fixed choke tick count |
| **Adaptive** | Choke scales with movement speed (2 idle, max/2 walking, max running) |
| **On-Peek** | Full choke while strafing, release on attack |
| **Random** | Random choke within range |
| Visualization | On-screen bar showing choke count / max |

### Visuals

| Module | Description |
|--------|-------------|
| **Player ESP** | Box, skeleton (17-bone rig), snaplines, health bars, dormant player tracking with velocity extrapolation, configurable colors per element, distance filter, FOV circle |
| **Entity ESP** | 7 entity types (printers, shipments, drugs, doors, weapons, money, vehicles) with per-type color config, diamond markers, owner labels, money display, shipment countdown timers, entity health bars, distance in meters |
| **Chams** | Visible (green) and hidden (red) material overrides via DrawModelExecute, configurable colors |
| **Sound ESP** | Positional audio visualization on screen |
| **Threat Radar** | Top-right minimap showing nearby players as colored dots (green normal, red admin), yaw-rotated, configurable zoom |
| **Heatmap** | Accumulated player position heatmap overlay |
| **X-Ray** | See-through material rendering |
| **Aim Lines** | Predicted aim trajectory visualization |
| **Prediction Ghosts** | Translucent skeleton at predicted future position with dashed leader lines |
| **Crosshair Info** | Entity info under crosshair |
| **Hitmarker** | Hit confirmation indicator |
| **Kill Sound** | Audio feedback on kills |
| **Killfeed** | Enhanced kill tracking |
| **Killstreak** | Streak counter overlay |
| **Damage Log** | Damage event log with timestamps |
| **Fullbright** | `render.SetLightingMode(2)` via Lua |
| **Night Vision** | Brightness enhancement |

### Intel & ESP Badges

| Feature | Description |
|---------|-------------|
| **Admin Detection** | Real-time admin/superadmin badge on ESP, alert on admin join |
| **Spectator Detection** | Observer mode + target tracking, spectator count in HUD |
| **Money Tracker** | DarkRP wallet display per player |
| **Wanted Status** | Warrant/wanted badge on player ESP |
| **Weapon Intel** | Active weapon + full inventory list per player |
| **Job/Gang/Faction** | DarkRP role, gang, faction, organization display |
| **RP Name** | Both Steam name and DarkRP RP name |

### Movement

| Module | Description |
|--------|-------------|
| **Bunny Hop** | Auto-jump on ground contact via CreateMove |
| **Auto Strafe** | Three modes: legit (mouse-based), silent (optimal angle calc), directional (WASD-relative) |
| **Edge Jump** | Detects walking off edges and auto-jumps for max distance, optional auto-duck |
| **Fast Stop** | Computes counter-strafe angle from velocity and applies instant stop |

### Voice Intelligence

A full voice communication analysis suite (`voice_exploits.hpp`):

| Feature | Description |
|---------|-------------|
| **Channel Intercept** | Hear all voice channels regardless of team/proximity restrictions |
| **Force Unmute** | Override per-player mute settings |
| **Volume Boost** | Configurable voice amplification (up to 3x) |
| **Activity ESP** | Volume bars above speaking players with duration, job labels |
| **Direction Arrows** | Off-screen pulsing arrows pointing toward speakers not in view |
| **Raid Alert** | Flashing "VOICE RAID ALERT" when 2+ enemies talk near you |
| **Social Mapper** | Tracks who talks together (overlap count), displayed in intel panel |
| **Pattern Profiler** | Per-player stats: total talk time, session count, avg duration, recency |
| **Voice Radar** | Mini-radar showing speaker positions, yaw-rotated, pulsing dots |
| **Freecam Proximity** | Only hear voices near freecam position (configurable radius) |
| **File Logger** | Log voice activity to disk |

### Autonomous Bot System

A complete autonomous player bot spanning four modules:

#### Bot Combat (`bot_combat.hpp`)
- **Threat Scanner**: Scans all players, computes danger score (distance, aim direction, weapon status, facing angle), sorts by threat level
- **Engagement Rules**: Configurable engage distance, disengage health threshold, fight-back-only mode, protect-follow-target mode, headshot-only option
- **Auto-Aim**: Smooth aim interpolation toward target bones, 3-degree on-target threshold for firing
- **Cover System**: 8-direction ray sampling at 200u radius, scores positions by line-of-sight blockage + distance from threat
- **Health Monitoring**: Tracks damage taken, detects "under fire" state (2s decay), damage rate calculation

#### Bot Navigation (`bot_nav.hpp`)
- **Nav Graph**: 512-node ring buffer, auto-recorded from player movement (100u min spacing), 400u connection radius, walkability-verified connections
- **A\* Pathfinding**: Stack-allocated open/closed lists (zero heap), euclidean heuristic, water/ladder edge penalties, 100-iteration cap
- **Path Smoother**: Iterative waypoint removal via dual-height visibility checks (waist + feet)
- **Terrain Analysis**: Ground detection, slope angle measurement, water detection, ladder detection
- **Fall Damage Calculation**: Source engine model (256u safe zone, 10 damage per 256u after)
- **Stair Detection**: Multi-sample step height analysis (8 samples at 24u spacing)
- **Swimming Handler**: Auto-jump to surface, duck to dive, alternating jump to tread water
- **Smart Stuck Recovery**: 8-stage escalation (strafe L/R -> jump-strafe L/R -> backpedal -> duck-move -> random burst -> noclip spam), auto-door (+USE) on blocked

#### Bot Tasks (`bot_tasks.hpp`)
Full finite state machine with six modes:

| Mode | Behavior |
|------|----------|
| **Idle** | Stand still with slow random look sway |
| **Follow** | Breadcrumb trail following with A* nav fallback |
| **Guard** | Hold position, return when drifting, face threats in radius |
| **Patrol** | Walk waypoint sequence (loop or ping-pong), animated path visualization |
| **Farm** | Auto-scan for loot entities (money/printers/weapons), walk to nearest, +USE to collect, priority-sorted targeting |
| **Flee** | Run away from danger source for configurable duration, sprint enabled |

Shared infrastructure: 16-ray obstacle avoidance (240-degree FOV), smooth yaw steering, auto-jump on steps/gaps, auto-door (+USE) on blocked paths.

#### Bot Visuals (`bot_visuals.hpp`)
- **Status HUD**: Mode label, health bar, under-fire warning (flashing), target info, threat count, mode-specific details
- **Path Visualization**: Bezier curves between breadcrumbs with gradient coloring (near=cyan, far=blue), animated pulse dot traveling the path, arrow head at target
- **Threat Indicators**: Off-screen directional arrows colored by danger level (red/yellow/gray), distance labels
- **Guard Radius**: Projected dashed circle on ground, animated rotation, "ON POST" / "RETURN TO POST" label
- **Patrol Path**: Waypoint dots (completed=filled, current=pulsing, upcoming=hollow), connecting lines, animated dashed line to next waypoint
- **Farm Markers**: Pulsing glow on nearest target, $ and * icons, distance labels
- **Minimap**: 180px rotating radar with compass marks (N/E/S/W), threat dots, patrol waypoints, guard radius, nav path overlay

### Follow Bot (`follow_bot.hpp`)
- 64-slot breadcrumb trail from target's movement
- Nav graph A* fallback when direct path is blocked
- Configurable follow/stop/sprint distances
- Stuck detection with progressive recovery
- Mimic mode (copy target's movement style)
- Silent move (preserve view angles)
- Auto-jump and auto-door

### DarkRP Automation

Nine modular Lua script batches:

| Batch | Features |
|-------|----------|
| **Batch 1** | Warrant shield (auto-close fading doors when warranted + HUD warning), anti-arrest system |
| **Batch 2** | Base alarm, auto-bounty management |
| **Batch 3** | Loot vacuum (auto-pickup money/weapons/items within range, class-based filtering) |
| **Batch 4** | Hitman intel (query active hit target info), door exploit |
| **Batch 5** | Shipment scanner, proximity alerts |
| **Batch 6** | Permission bypass engine (detour DarkRP restriction functions: canBuyPistol, canBuyShipment, canPocket, etc.) |
| **Batch 7** | Entity telekinesis (pull/push entities with R/T keys, extended USE range, through-wall targeting) |
| **Batch 8** | Salary exploit (accelerate DarkRP_PayDay timer to 1s), salary arbitrage (rapid job switch for salary collection) |
| **Batch 9** | Admin spoof (client-side IsAdmin/IsSuperAdmin/GetUserGroup overrides, ULib/SAM/FAdmin permission hooks) |

### Advanced Exploits (`exploit_batch_v3.hpp`)

| Exploit | Description |
|---------|-------------|
| **HDR Stacking** | Multi-pass DrawColorModify for wallhack-like glow effect |
| **Material Glow** | Self-illuminating halo outlines on players via PreDrawHalos |
| **Render Override** | Re-render players ignoring Z-depth (see through walls) via PostDrawOpaqueRenderables |
| **Physics Prediction** | Draw spheres at predicted future positions (1-3 tick lookahead with collision) |
| **Tip/Donate Exploit** | Scan net.Receivers for tip/donate messages, rapid-fire exploit attempts |
| **Trade Sniper** | Scan for trade system net messages and globals |
| **Hit/Bounty Dupe** | Race-condition rapid bounty placement |
| **Salary Arbitrage** | Rapid high-salary job switch cycle with auto-restore |
| **Error Log Sanitizer** | Detour ErrorNoHalt/Msg/MsgC to block messages containing cheat keywords |
| **CRC/Checksum Spoof** | Cache clean CRC values, hide cheat files from file.Exists/file.Size |
| **Stack Trace Spoof** | Rewrite debug.traceback/debug.getinfo to replace RunString/CompileString sources with fake autorun paths |
| **Hook Table Sanitizer** | Detour hook.GetTable and timer.GetTable to filter out _fdll_ entries |
| **Constraint Catapult** | Weld to physics prop + velocity burst for launch (2s ride) |
| **Vehicle Speed Boost** | Multiply vehicle velocity via Think hook (3x default) |
| **Fake Admin Panel** | VGUI phishing menu mimicking ULX admin panel |
| **Animation Override** | Force custom animations: T-pose, zombie crawl, dance, dead |
| **ConVar Callback Trigger** | Rapid toggle server convars to fire callbacks |
| **Sound Mask** | Mute own footsteps, amplify enemy sounds (2.5x volume, -20 sound level) |
| **Prop Shield** | Spawn near-invisible clientside props as barriers |
| **Death Cam** | Free-roam camera during death (WASD + mouse, Shift speed) |

### Additional World Exploits

| Feature | Description |
|---------|-------------|
| **Night Vision** | Enhanced brightness mode |
| **Anti-AFK** | Prevent idle kick |
| **Silent Walk** | Muted movement |
| **Slide Walk** | Slide movement exploit |
| **Lockpick Auto** | Auto-lockpick doors |
| **Prop Alert** | Notification when props placed near you |
| **Prop Fly** | Physics prop flight |
| **Prop Kill** | Prop-based player elimination |
| **Entity Magnet** | Pull entities toward you |
| **Ghost Mode** | Visibility reduction |
| **Infinite Ammo** | Unlimited ammunition via Lua |
| **No Recoil (Lua)** | Lua-side recoil elimination |
| **Anti-Crash** | Crash prevention |
| **Vote Bot** | Auto-vote manipulation |
| **Puppet Recording** | Record and replay movement sequences |
| **Material Wallhack** | Material-based see-through |
| **Low Gravity** | Gravity modification |
| **Sound Spam** | Audio spam |
| **Auto Buy** | Automatic purchasing |
| **Net Sniffer** | Network message inspection |
| **Duplication Exploits** | Net capture, buy capture, auto-loop, burst mode (configurable count/delay/interval) |

### Stealth & Anti-Detection

13 layered stealth systems (`stealth.hpp`):

| System | Description |
|--------|-------------|
| **Panic Key** | END key -- instant atomic toggle of all visible features |
| **Spectator Detection** | Per-frame observer mode + target scan, auto-panic when spectated, auto-unpanic when clear |
| **Spectator Cloak** | Selective per-feature disable (not full panic) when spectated |
| **Screenshot Cleaner** | Detour render.Capture / render.ReadPixels / render.CapturePixels to return nil |
| **Anti-Screenshot (Expanded)** | Additional surface.GetTextureID interception |
| **Anti-Anticheat** | Strip existing AC hooks by keyword (anticheat, anti_cheat, ac_, screengrab, cheatdetect, cac_), detour hook.Add to block future AC hook registration |
| **AC Bypass** | Full gAC/CAC/StackAC scanning hook bypass |
| **Anti-Kick** | Auto-vote-no on kick votes + block kick concommand |
| **Name Steal Cycle** | Cycle through other players' names every 30s |
| **Fake Death** | Periodic fake death events while alive |
| **Admin Bypass** | Hide from ULX/SAM/FAdmin/ServerGuard player lists |
| **Fullbright** | render.SetLightingMode toggle with clean install/uninstall |
| **Recording Mode** | Disable all overlays for clean footage |

### Death Replay (`death_replay.hpp`)

256-frame ring buffer (~7-8 seconds at 33 tick) recording per-player head position, origin, and health each tick. On death, plays back a ghost visualization of the last N seconds. Dismissable with ESC/Space/Enter/click.

### Misc Features

| Feature | Description |
|---------|-------------|
| **Freecam** | Lua-based free camera (F3 toggle, WASD + Shift for speed, CalcView hook) |
| **Third Person** | Smooth third-person view via CalcView with traceline collision |
| **Custom Crosshair** | Configurable crosshair overlay |
| **Player Profiler** | Per-player statistics tracking |
| **Printer Monitor** | DarkRP printer location and money tracking |
| **Door Memory** | Remember door ownership and access |
| **Spawn Detection** | Alert when players spawn nearby |
| **Waypoints** | Persistent world-space markers |
| **Net Panel** | Network diagnostics display |
| **Rage Mode** | Aggressive play mode toggle |
| **Auto Disguise** | DarkRP disguise automation |
| **Chat on Death** | Configurable death message broadcast |

## Intel System

The intel pipeline runs a large embedded Lua query (`intel_lua.hpp`) every frame that:

1. Iterates all players, extracting: RP name, job, gang/faction/organization, active weapon (with print name resolution), full weapon inventory, admin/superadmin status, observer mode/target, DarkRP money, wanted status
2. Scans all entities (up to 512) for: printers (with stored money from multiple sources), shipments (with contents and count), drugs/bitcoin miners, owned doors, dropped weapons, money bags, vehicles (with driver detection)
3. Double-buffers results into `config::g_entBuf` with atomic index swap for lock-free render-thread reads

## Interfaces

| Interface | Version String | Purpose |
|-----------|---------------|---------|
| IClientEntityList | `VClientEntityList003` | Entity iteration |
| IBaseClientDLL | `VClient017` | Client hooks |
| CEngineClient | `VEngineClient015` | Console commands, IsInGame |
| CLuaShared | `LUASHARED003` | Lua state access |
| IPrediction | `VClientPrediction001` | Movement prediction |
| IEngineTrace | `EngineTraceClient003` | Ray casting (visibility, cover, navigation) |
| ICVar | `VEngineCvar007` | ConVar manipulation (sv_cheats, sv_allowcslua, fov_desired) |
| CModelRender | `VEngineModel016` | Chams via DrawModelExecute |
| IGameEventManager2 | -- | Damage events (player_hurt listener) |
| CMaterialSystem | -- | Material creation for render overrides |
| IMatRenderContext | -- | Render state management |
| CGameMovement | -- | Movement calculation |
| IMoveHelper | -- | Movement data |
| IVDebugOverlay | -- | Debug rendering |
| GlobalVars | -- | curtime, frametime, interval_per_tick, maxClients |

## Lua Engine

### State Acquisition (`lua_loader.cpp`)
Automatically discovers `lua_State` offset by disassembling `CLuaInterface::Top()` -- scans for `REX.W MOV [rcx+offset]` pattern to find the state pointer.

### Execution Modes
| Function | Behavior |
|----------|----------|
| `Execute()` | Fire-and-forget script run |
| `ExecuteFile()` | Load .lua file from disk |
| `ExecuteAndGetResult()` | Execute and return string result |
| `QueueScript()` | Async queued execution via ProcessQueue() |
| `QueueRun()` / `QueueQuery()` | Batch execution with result capture |

### Embedded Scripts (~320 KB in `lua_scripts.hpp`)
- Freecam (CalcView + Think + InputMouseApply)
- Screenshot cleaner (render.Capture / ReadPixels / CapturePixels override)
- Third-person camera (CalcView + traceline)
- Death message broadcast
- Admin/spectator detection hooks
- 20+ exploit scripts (HDR stacking through death cam)
- Voice intelligence system
- Anti-anticheat hook stripping and detour

## UI

ImGui-based overlay toggled with **INSERT**:

- **Theme**: Dark background with cyan (`#00B4D8`) accents, 10px window rounding, 6px child rounding
- **Layout**: Tab-based navigation with animated underline transitions
- **Effects**: Header particle system, menu slide-in animation
- **Audio**: UI sound effects on interaction (button14/15/17/18/24 via engine)
- **Fonts**: Dual FreeType rendering (defaultFont + editorFont)
- **Editor**: ImGuiColorTextEdit with Lua syntax highlighting for live script execution
- **Persistence**: Window positions saved to `friendlydll_window.cfg`
- **Input**: Mouse/keyboard blocking when menu is open, WndProc subclass for INSERT key capture
- **Custom widgets**: `ui_widgets.hpp` (specialized controls), `ui_anim.hpp` (lerp/easing), `ui_theme.hpp` (color scheme), `ui_window.hpp` (window management), `menu_anim.hpp` (particle effects + tab transitions)

## Dependencies

| Library | Purpose |
|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Overlay UI + tables + widgets |
| [MinHook](https://github.com/TsudaKageWorked/minhook) | x86/x64 API hooking (hde32/hde64 disassembler) |
| [Kiero](https://github.com/Rebzzel/kiero) | DirectX vtable hook bootstrapping |
| [FreeType](https://freetype.org/) | Font rasterization (`imgui_freetype.cpp`) |
| [spdlog](https://github.com/gabime/spdlog) | Async logging + file sink (`Friendlydll_error.log`) |
| [Boost.Regex](https://www.boost.org/) | Pattern matching |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Lua editor with syntax highlighting |

## Building

**Requirements**: Visual Studio 2019+, Windows SDK, C++17.

1. Open the `.vcxproj` in Visual Studio.
2. Set configuration to **Release | x86** (Garry's Mod is 32-bit) or **x64** as needed.
3. Build. Output is `Friendlydll.dll`.

## Injection

Inject the compiled DLL into a running `gmod.exe` process. The DLL self-initializes on `DLL_PROCESS_ATTACH`:

1. Allocates debug console (`Friendlydll-GMOD Console`) with Ctrl+C/Close handler
2. Initializes MinHook
3. Hooks D3D9 Present via Kiero
4. Captures 15 game interfaces
5. Resolves netvars with offset validation (dumps to `gmod_netvars.txt`, warns on offset shifts)
6. Installs CreateMove / DrawModelExecute / FrameStageNotify hooks
7. Discovers Lua state via disassembly of CLuaInterface::Top()
8. Starts scheduler (100ms ConVar enforcement loop)
9. Registers `player_hurt` game event listener for damage tracking
10. Plays `hl1/ambience/particle_suck1.wav` as load confirmation

Press **END** to safely eject (graceful shutdown: unhook events -> stop scheduler -> unhook game -> unhook D3D9 -> uninitialize MinHook -> free console -> FreeLibraryAndExitThread).

## Controls

| Key | Action |
|-----|--------|
| INSERT | Toggle menu |
| END | Panic toggle / eject DLL |
| F3 | Toggle freecam |
| Mouse4 | Lag switch (hold) |
| Mouse5 | Doubletap charge |
| R | Telekinesis pull (DarkRP) |
| T | Telekinesis push (DarkRP) |

## Config

200+ settings exposed via `config.hpp` as inline variables with `std::atomic<>` for cross-thread state. Categories:

- Movement (bunnyhop, autostrafe modes, edge jump, fast stop)
- Aiming (aimbot, silent, autoshoot, FOV, smooth, bone, priority weights, target lock)
- ESP (snapline, box, chams, skeleton, dormant, radar, PVS forcing, per-element colors)
- Combat (triggerbot, backtrack, view punch, resolver)
- Entity ESP (7 entity types, per-type toggles and colors, health bars, timer max)
- HUD (minimap, FOV circle, fullbright, sound ESP, crosshair info)
- Intel (admin alert, spectator alert, money tracker, intel badges)
- Stealth (13 toggles from panic key to AC bypass)
- Exploits (30+ toggles for world, DarkRP, duplication, and v3 exploits)
- Anti-aim (6 yaw modes, 5 pitch modes, desync offset)
- Bot (combat toggles, engagement config, task mode, farm/guard/patrol settings)
- Voice (11 feature toggles, boost level, freecam proximity radius)
- Player list (whitelist/blacklist mode with per-entity-index set)

Serialization via `config_io.hpp`.

## Data Structures

### Double-Buffered Bone Cache
```
BoneRecord[128] x 2 buffers
  - Matrix3x4 bones[128] (full skeleton)
  - absOrigin, health, distance
  - name, rpName, job, weapon, weaponList, gang
  - isAdmin, isSuperAdmin, observerMode, observerTarget
  - money, isWanted, dormant, noBones
  Atomic index swap: g_boneReadIdx
```

### Backtrack Ring Buffer
```
BacktrackTick[128 players][12 ticks]
  - Matrix3x4 bones[128], origin, simtime
  Ring head advances each tick, 200ms age window
```

### Entity ESP Records
```
EntRecord[512] x 2 buffers
  - pos, label, owner, type (0-6), money, health, maxHealth, distance
  Atomic index swap: g_entReadIdx
```

### View Matrix
```
VMatrix[2], screenW[2], screenH[2], cameraOrigin[2]
  Written by CreateMove, read by Present via atomic index
  WorldToScreen() uses perspective division
```

## Design Notes

- **Header-only features**: All 62 feature modules live in `core/*.hpp` as inline implementations. Zero link overhead, all optimization happens at compile time.
- **Double-buffered everything**: Bone cache, entity records, view matrices, and crosshair info all use atomic-swapped double buffers for lock-free reads from the render thread.
- **Thread safety**: `std::atomic<bool>` for `g_inGame`, `g_panic`, `g_requestEject`, `g_beingSpectated`; `std::atomic<int>` for `g_spectatorCount` and all buffer read indices.
- **Lazy Lua installation**: Stealth features (screenshot cleaner, anti-AC, fake death, etc.) install/uninstall reactively per-frame based on config toggles via `stealth::StealthInit()`.
- **Bot architecture**: Four-module split (combat/nav/tasks/visuals) with shared trace helpers, no cross-module circular dependencies, all state in inline globals.
- **Nav graph**: Fixed-size ring buffer (512 nodes) avoids heap allocation. A* uses stack-allocated open/closed lists. Path smoothing removes redundant waypoints via dual-height visibility checks.
- **Intel pipeline**: Single large Lua query returns tab-delimited data for all players + entities in one execution, parsed C++-side with zero Lua callbacks.
- **Safe ejection**: Multi-phase shutdown (clear state -> remove event listeners -> stop scheduler -> unhook game -> unhook D3D9 -> uninitialize MinHook) with Sleep guards between phases.

---

**For educational and research purposes only.**
