# trace — KiCad, terminal-first

**trace** is a fork of [KiCad](https://kicad.org/) (the open-source EDA suite: schematic
capture + PCB layout) that tracks the upstream KiCad 10 development tree
(`KiCad/kicad-source-mirror`) and adds **headless, terminal-driven, and agent-oriented**
capabilities on top of it.

- Repo: `github.com/asappcb/trace` · Version: `10.99.0` (KiCad 10 dev tree)
- Everything not listed under [What trace adds](#what-trace-adds-over-upstream-kicad) tracks
  upstream KiCad and behaves identically.
- The headless entry point is **`kicad-cli`**. Live automation goes through the **IPC API**
  (protobuf over nng); the legacy SWIG Python bindings have been removed.

> **Working in this repo with an AI tool?** Start with [`CLAUDE.md`](CLAUDE.md) — a compact
> architecture + build map written for coding agents.

---

## What trace adds over upstream KiCad

Verified fork-exclusive features (present in `asappcb/trace`, not in upstream KiCad):

| Feature | What it does | Surface |
|---|---|---|
| **`kicad-cli pcb optimize-swaps`** | Headless gate/pin-swap optimizer — cuts ratsnest length by swapping electrically-interchangeable gates/pins, writes the result back | CLI + `job_pcb_optimize_swaps` |
| **`kicad-cli release create` / `release verify`** | Versioned, hash-manifested release bundles with a verify step | CLI + `common/release/` |
| **Command palette (Ctrl/Cmd-K)** | Fuzzy "go to anything" + action runner across pcbnew & eeschema — navigation, layer switching, MRU quick-place, context-aware enablement | `common/dialogs/dialog_command_palette` + `kicad_fuzzy_match` |
| **Backdrill / post-machining** | First-class back-drilling: dedicated DRC constraints (clearance, hole-to-hole, stub length, post-machining depth), plus ODB++, IPC-2581, Gerber, and STEP export | `drc_test_provider_backdrill`, exporters, `backdrill_via_edit` |
| **Gate/pin-swap equivalence transport** | Carries schematic gate/pin-swap equivalence to the board so the optimizer and interactive swaps honor the units-interchangeable flag | `pcbnew/gate_swap`, `board_swap_metrics` |
| **Headless board→SVG render core** | Reusable in-memory `BOARD → SVG` path (no GUI, no output file to manage) — groundwork for a render-query API | `pcbnew/render_board` |
| **IPC correctness fixes** | API `modify` preserves persisted fields the proto doesn't carry (no silent wipes); IPC-2581 always emits a valid `layerFunction` | `pcbnew/api/`, exporters |
| **Native GitHub Actions CI** | Full build + QA on GitHub (Fedora container), affected-module fast lane for PRs — replaces the upstream one-way-mirror lockdown | `.github/workflows/` |

Each substantive fix lands with a Boost.Test regression under `qa/tests/`. This fork also
carries a curated memory-safety / correctness-hardening series (use-after-free, dangling
refs, ownership → `unique_ptr`, re-entrancy, bounds) authored by KiCad core devs.

---

## `kicad-cli` — headless command reference

Everything KiCad can do without a GUI runs through `kicad-cli`. **Fork-only** commands are
tagged `[FORK]`; the rest are standard KiCad and documented at
[docs.kicad.org/en/cli](https://docs.kicad.org/en/master/cli/).

```
kicad-cli
├── version                         # print version (--format plain|commit|about)
├── pcb
│   ├── drc         BOARD           # design-rule check      → --format json|text
│   ├── diff        A B             # semantic board diff     → --format json|text|svg|png  (json default)
│   ├── render      BOARD           # raytraced/board render
│   ├── import      FILE            # import foreign board formats
│   ├── upgrade     BOARD           # migrate to current file format
│   ├── optimize-swaps  BOARD       # [FORK] gate/pin-swap optimizer, writes back
│   └── export
│       ├── svg png pdf ps dxf hpgl gerbers drill pos gencad ipc2581 ipcd356 odb stats
│       └── step glb vrml xao ply stl stepz            # 3D / MCAD
├── sch
│   ├── erc         SCH             # electrical-rules check → --format json|text
│   ├── diff        A B             # semantic schematic diff
│   ├── import / upgrade
│   └── export      bom pythonbom netlist dxf pdf svg ...
├── sym     ( diff | upgrade | export svg )
├── fp      ( diff | upgrade | export svg )
├── gerber  ( info | convert-png | diff )              # gerber inspection
├── release                        # [FORK]
│   ├── create                     # build a versioned, hash-manifested release bundle
│   └── verify                     # verify a bundle against its manifest
├── jobset run  FILE.kicad_jobset  # run a saved batch of jobs
└── mergetool / git-mergedriver    # native diff/merge for .kicad_* files (git integration)
```

### Conventions that matter for scripting

- **Exit codes are meaningful.** `0` = success; DRC/ERC/diff return **non-zero when
  violations/differences exist** (e.g. `pcb diff` returns `5` on differences), so you can gate
  on them directly.
- **`--output` is required** for image/plot formats and **never overwrites the input in
  place** — the tool refuses rather than clobber your source.
- **Prefer `--format json`** where offered (`drc`, `erc`, `diff`) for machine-readable output
  instead of scraping text.

### Examples

```bash
# DRC to JSON; fail the script if there are violations
kicad-cli pcb drc board.kicad_pcb --format json -o drc.json || echo "DRC found issues"

# Semantic board diff between two revisions (exit 5 == they differ)
kicad-cli pcb diff old.kicad_pcb new.kicad_pcb --format json -o diff.json

# Reduce ratsnest by swapping interchangeable gates/pins   [FORK]
kicad-cli pcb optimize-swaps board.kicad_pcb -o optimized.kicad_pcb

# Fabrication outputs
kicad-cli pcb export gerbers board.kicad_pcb -o gerbers/
kicad-cli pcb export drill   board.kicad_pcb -o gerbers/
kicad-cli pcb export step    board.kicad_pcb -o board.step

# Build and verify a release bundle   [FORK]
kicad-cli release create  --output dist/
kicad-cli release verify  dist/manifest.json
```

---

## For AI agents & automation

- **Read [`CLAUDE.md`](CLAUDE.md) first** — it maps the architecture (KIWAY software bus, GAL
  rendering, tool framework, transactional COMMIT undo model, IO plugin registries) and the
  exact build invocation, sized for a coding agent's context.
- **Two automation surfaces:**
  - **`kicad-cli`** — one-shot, headless, scriptable (the reference above). Best for CI,
    batch export, checks, and fabrication.
  - **IPC API** — protobuf3 schemas (`api/proto/{common,board,schematic}`) transported over
    nng, for *live* manipulation of an open document. This replaces SWIG, which is removed
    (no `KICAD_SCRIPTING`, no `.i` files).
- **File formats** are s-expressions (`.kicad_pcb`, `.kicad_sch`, …). Read them via the tools
  above, not by hand — `kicad-cli ... diff --format json` and the IPC API give you structured
  views that don't rot when the format version bumps.
- **Known headless gaps** are tracked as `enhancement`-labelled issues on `asappcb/trace`
  (e.g. zone-fill write-back, scriptable board edits, `pcb export json`, connectivity/ratsnest
  report). Check open issues before assuming a capability is missing.

---

## Build

Full details in [`CLAUDE.md`](CLAUDE.md). In short: CMake ≥3.22, C++20, and the external deps
(wxWidgets ≥3.2, Boost, OpenCASCADE, ngspice, Protobuf + Abseil, nng, libgit2, Cairo stack).
Upstream build docs: [dev-docs.kicad.org/en/build](https://dev-docs.kicad.org/en/build/).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
# QA (Boost.Test under CTest):
ctest --test-dir build/qa
```

Outputs: the GUI launchers (`kicad`, `pcbnew`, `eeschema`, `gerbview`, …), the console
**`kicad-cli`**, and the KIFACE editor modules (`_pcbnew.kiface`, `_eeschema.kiface`, …).

---

## Upstream KiCad resources

trace tracks upstream closely; upstream documentation applies to everything outside the fork
feature list above.

- [KiCad website](https://kicad.org/) · [User docs](https://docs.kicad.org/) ·
  [CLI docs](https://docs.kicad.org/en/master/cli/)
- [Developer docs](https://dev-docs.kicad.org) ·
  [Contribution guide](https://dev-docs.kicad.org/en/contribute/) (upstream contributions go
  to KiCad's GitLab)
- [Forum](https://forum.kicad.info/)

## License

GPLv3 — see [copyright.h](copyright.h) and the `LICENSE` files. trace is a downstream fork of
KiCad and carries the same license.
