# Toolkit sync: hp2_recomp fixes in hp3

hp3 is forked from hp1_recomp (commit 63274de). hp2_recomp shares the xboxrecomp toolkit but drifted. This file
records, per difference, what hp3 took and why. Triage: Codex (read-only), 2026-10-30, over
`diff -ruN hp3/{src,tools,cmake,include,tests} hp2/...`; decisions by Claude Code.

| Area | hp2 change | hp3 decision |
|---|---|---|
| `kernel.h`, `kernel_xbox.c` | `XC_FACTORY_AV_REGION` (0x103) answered NTSC-M 60 Hz | **taken**. hp2's comment names PoA: it times frames by the video standard (25 PAL / 30 NTSC) |
| `video/fb_present.c` | bare Alt/F10 (`SC_KEYMENU`, l=0) no longer enters the modal menu loop | **taken** |
| `kernel/nv2a_pb_exec.c` | framebuffer pick prefers the display-mode pitch, then size | **taken** |
| `kernel/kernel_rtl.c` | per-thread cache of the last critical section looked up | **taken**; hp2's poll-yield before `RtlEnterCriticalSection` is CoS scheduling policy, **not taken** |
| `kernel/kernel_thunks.c` | `XBOX_KERNEL_LOG` names the log file | **merged** with hp1's `RECOMP_DEBUG_LOGS=0` (kept) |
| `xbox_memory_layout.{c,h}` | heap-reclaim comments match the default (on) | **taken** (comments only) |
| `xbox_memory_layout.c` | high-address candidates + fixed-window preflight | **deferred** (M2): needs a fix first, since POSIX `VirtualQuery` returns RegionSize 0 and the scan never advances |
| `kernel_bridge.c` | guest thread object -> host HANDLE map, priority routing | **deferred** (M2): must keep hp1's thread IDs/TIBs, needs locking and lifetime handling |
| `kernel_bridge.c`, `kernel_ordinal_names.h` | slow-call timing, handle->filename, ordinal names, free/read-delay diagnostics | **deferred** (M2 bring-up, as opt-in) |
| `kernel_bridge.c`, `kernel_thread.c` | shared-core yields, boost suppression, time-critical sleep telemetry | **not taken**: tuned for CoS |
| `kernel_bridge.c`, `xbox_memory_layout.*` | removes hp1's guest thread IDs, per-worker KTHREAD, `xbox_SetVblankHook` | **kept hp1**: hp2 lost working code (the D3D8 HLE calls the vblank hook) |
| `tools/recomp/lifter.py` | removes `WBINVD`/`INVD` handling | **kept hp1**: both preserve EFLAGS and lower to no-ops; hp2 regressed |
| `tools/kcap/` | XBDM kernel-call capture extension (incomplete source) | **not taken** until complete |
| `src/input/CMakeLists.txt` | comment only | kept hp1 |
