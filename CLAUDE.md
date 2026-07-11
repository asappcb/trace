# CLAUDE.md — KiCad (`trace` fork)

## What this is
KiCad, the open-source EDA suite (schematic capture + PCB layout). This checkout is the
`github.com/asappcb/trace` fork tracking `upstream = KiCad/kicad-source-mirror`.
Version `10.99.0-unknown` (`cmake/KiCadVersion.cmake`) — the KiCad 10 development tree.

- ~18.5k files; ~2,970 `.cpp` + ~3,480 `.h` first-party C++20, plus ~2,400 vendored files.
- The GitHub side is a **one-way mirror**; PRs are auto-closed (`.github/workflows/lockdown.yml`).
  Real CI is on GitLab (`.gitlab-ci.yml` + `.gitlab/`).
- **Fork character:** recent commits are a curated **memory-safety / correctness hardening**
  series (use-after-free, dangling refs, ownership → `unique_ptr`, null-guards, re-entrancy,
  bounds). Each substantive fix lands with a Boost.Test regression under `qa/tests/`.
  Authored by KiCad core devs — this fork tracks upstream closely.

## Build
- CMake ≥3.28.1 (MSVC) / ≥3.22 (else); **C++20**; default build type `Release`
  (`QABUILD` config keeps asserts for QA). Driver: top-level `CMakeLists.txt` (~1,150 lines).
- `add_subdirectory` order = dependency order:
  `api → resources → thirdparty → libs → common → 3d-viewer → eeschema → gerbview → pcbnew →
  pagelayout_editor → bitmap2component → pcb_calculator → plugins → cvpcb → kicad → tools → utils → qa`.
- Heavyweight deps are external / vcpkg: **wxWidgets ≥3.2, Boost ≥1.71, OpenCASCADE ≥7.6 (OCC),
  ngspice, Protobuf ≥3.12 + Abseil, nng, libgit2, Cairo/Pixman/Freetype/HarfBuzz**.
  Small utility/geometry/parser libs are vendored in `thirdparty/` (clipper2, pegtl, fmt,
  nlohmann_json, potrace, nanosvg, glad, zint, turtle, sentry-native, libcontext, …).
  Package mgmt: `vcpkg.json` (custom KiCad registry) + Debian `install-deps.sh`.
  Note: `vcpkg.json` version string "8.99" is stale vs the real 10.99.
- **Output:** thin GUI launchers (`kicad`, `eeschema`, `pcbnew`, `gerbview`, `pl_editor`,
  `cvpcb`, `pcb_calculator`, `bitmap2component`, console `kicad-cli`). Real logic lives in
  **KIFACE modules** (`_pcbnew.kiface`, `_eeschema.kiface`, … — native DLLs on Windows).
  Shared libs: **`kicommon`** (linked by everything) and **`kiapi`** (protobuf IPC).
- **SWIG Python bindings are removed** (no `KICAD_SCRIPTING`, no `.i` files). Automation is the
  new IPC API. Python remains only as build tooling + footprint `wizards`.

## Core architecture (read `common/` + `include/` first — apps are specializations)
**One process, a software bus, pluggable editors.** Editors are `.kiface` DSOs loaded into one
process and wired via **KIWAY** (in-process, no IPC between them). See `include/kiway.h`.
```
PGM_BASE (process; owns SETTINGS_MANAGER)
  └─ KIWAY (one per project; owns PROJECT; dlopens .kiface DSOs; routes mail)   common/kiway.cpp
       └─ KIFACE (factory each DSO exports; ABI-frozen vtable)
            └─ KIWAY_PLAYER (an editor main window)
```
- **Cross-editor comms = ExpressMail:** `KIWAY::ExpressMail(dest, MAIL_T, payload)` →
  target's `KiwayMailIn()`. Vocabulary in `include/mail_type.h` (`MAIL_CROSS_PROBE`,
  `MAIL_SCH_UPDATE`/`MAIL_PCB_UPDATE` annotation, `MAIL_ASSIGN_FOOTPRINTS`, `MAIL_EESCHEMA_NETLIST`).
- **Frame hierarchy:** `wxFrame + TOOLS_HOLDER + KIWAY_HOLDER → EDA_BASE_FRAME → KIWAY_PLAYER →
  EDA_DRAW_FRAME` (base for graphical editors).
- **Rendering (model/view; items never draw to wx directly):** `EDA_ITEM` (also a `VIEW_ITEM`) →
  `VIEW` (R-tree + layers, `common/view/`) → `PAINTER` (per-app subclass, e.g. `pcb_painter.cpp`)
  → **GAL** graphics abstraction (`common/gal/` OpenGL + Cairo backends, hot-swappable).
  3D board view is a separate stack in `3d-viewer/`.
- **Tool framework (coroutine-based, wx-decoupled, `common/tool/`):**
  `wx event → TOOL_DISPATCHER → TOOL_EVENT → TOOL_MANAGER → tool coroutine / TOOL_ACTION`.
  Suite-wide actions in `include/tool/actions.h`; per-app tools in `<app>/tools/`.
- **Undo/redo = transactional `COMMIT`** (`include/commit.h`): fluent `Add`/`Remove`/`Modify` →
  `Push()`/`Revert()`. Subclasses `BOARD_COMMIT`, `SCH_COMMIT`. Tool actions receive a `COMMIT*`.
- **File I/O = plugin registry:** `SCH_IO_MGR`→`SCH_IO` (`eeschema/sch_io/{kicad_sexpr,altium,eagle,…}`),
  `PCB_IO_MGR`→`PCB_IO` (`pcbnew/pcb_io/…`), with format auto-detection.
- **Settings = JSON (nlohmann):** `JSON_SETTINGS` (PARAM bindings) under `SETTINGS_MANAGER`
  (owned by `PGM_BASE`).

## Module map / where things live
- `common/` (~856 src) + `include/` (~543) — the core described above; builds `kicommon`.
- `libs/` — dependency-free base: `core`, **`kimath`** (geometry/vectors), **`kiplatform`**
  (per-OS), `sexpr` (parser for `.kicad_*` files), `kinng` (NNG wrapper).
- `pcbnew/` (~1,270 src, largest) — PCB editor. Data model: `BOARD → FOOTPRINT → PAD/PADSTACK`,
  `PCB_TRACK/PCB_VIA`, `ZONE` + `zone_filler`. Key subsystems:
  - `router/` — Push & Shove interactive router (`pns_*`, 86 files, libcontext coroutines).
  - `drc/` — DRC engine: `drc_engine.cpp` + ~30 pluggable `drc_test_provider_*`, PEGTL rules,
    R-tree, interactive `drc/rule_editor/`.
  - `connectivity/` — net/cluster computation, feeds ratsnest + DRC.
- `eeschema/` (~800 src) — schematic editor. Data model: `SCHEMATIC → SCH_SHEET → SCH_SCREEN →
  SCH_SYMBOL`(instance)/`LIB_SYMBOL`(def). `connection_graph.cpp` = net/connectivity engine.
  - `sim/` — SPICE via ngspice (`sim_model_*` device families, plotting UI).
  - `erc/` — Electrical Rules Check. `netlist_exporters/`, `sch_io/` importers.
- `kicad/` — project manager launcher; `cli/` (`kicad-cli` + `jobs_runner`), `pcm/` (Plugin & Content Manager).
- `api/` — modern **IPC API**: protobuf3 schemas (`proto/{common,board,schematic}`) → `protoc`
  → shared `kiapi` lib, transported over **nng**. Replaces SWIG.
- `3d-viewer/`, `gerbview/`, `cvpcb/`, `pagelayout_editor/`, `pcb_calculator/`, `bitmap2component/`.
- `qa/` — **Boost.Test** + Turtle mocks, per-module executables (`qa_common`, `qa_eeschema`,
  `qa_pcbnew`, …) under CTest; `qa/fuzz/` fuzzes the s-expr PCB parser.

## Conventions
- Formatting enforced by clang-format (`_clang-format`) via `.githooks/` pre-commit chain.
- New source files use the GPLv3 header from `copyright.h`.
- Contributions go to KiCad's GitLab, not this GitHub mirror.
