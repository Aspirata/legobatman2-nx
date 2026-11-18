# Clear causal trace implementation plan

> Execute inline. This is the bounded diagnostic experiment already requested in
> `docs/research/switch-rendering-second-opinion.md`, not a renderer optimization.

Goal: capture complete GPU-thread frames 180 and 280, joining each guest draw or
resolve attempt to raw state, actual extent estimation, ownership requests and
emitted transfers, without GPU waits, queries or render-state changes.

Work in the existing checkout: its untracked SDK patches, generated sources and
local toolchain are required by the experiment. Preserve pre-edit SDK sources.

- [x] Verify run041 identities and compare FPS and transfers against run039.
- [x] Test a bounded binary recorder: disabled path, consecutive operations,
  exact payloads, overflow prefix, frame closure, two separated capture frames.
- [x] Add read-only SDK hooks at IssueDraw, extent estimation/interpreted exports,
  ownership requests, actual transfer and guest draws, and frame closure.
- [x] Record raw raster registers as deltas; clear shader programs and constants
  as snapshots. Do not interpret extra vertices solely for this diagnostic.
- [x] Validate decoder on actual recorder output; preserve dropped-data status.
- [x] Build ARM64 with existing required regressions. Keep timestamps compiled
  as before but runtime OFF; same-origin runtime OFF; restore scissor flag OFF.
- [x] Review the change, package, install and fully read back NRO/config.
- [x] Document results and request one normal opening-flight hardware run.

Result: run041 4.2021 vs039 4.1756 FPS, no useful gain established. New bounded
trace installed and full readback verified; hardware run042 is pending. Final
review approved after adding secondary host-depth sources, used texture fetch
descriptors and initialized ResolveInfo destination ranges. Build evidence in
`work/analysis/clear-causal-trace/build-verified.log`; console identities in
`verification.json`. No FPS improvement claimed for this diagnostic build.

Constraints: 4 MiB payload maximum per captured frame; one write at frame closure;
no per-draw file logging, query insertion, skipped rendering, additional queue
drains or changes to ownership/clear decisions. A trace is evidence of execution
and dependencies, not GPU latency or proof that a transfer can be removed.
