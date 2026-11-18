# Opening flight with GPU timing commands disabled

The MSAA ZCULL control037 did not show a sustained improvement. Return to the
normal035 renderer settings, retaining direct color resolve, dump/transfer pitch
specialization and guarded no-effect point elision. Compare against035, rather
than combining this measurement with the inconclusive no-ZCULL flag.

The diagnostic build now has an init-only `lb2_gpu_timestamps` flag, default true.
Setting it false stops recorder initialization before any Vulkan query allocation,
reset, timestamp write or result read. Per-draw shader-label map updates also stop.
Pipeline-creation name logs and CPU/native counters remain. SDK rendering code and
the two corrected Mesa driver archives are unchanged; rejected stencil split
remains unapplied and compiled OFF.

The disabled-path regression failed before this implementation, then passed with
the existing lifecycle/sampling/nonblocking cases:1003 checks total. Full Switch
build, regression suite and package validation passed. Bounded independent review
found no substantive issue. Isolated reversal of the existing SDK patches restores
the same normalized source baseline as037, and SDK public headers have no diff.

Installed and fully readback verified without dialogs:

- NRO70383813 bytes, SHA256
  `3b41891062ab6280abd3f080b8b659da87301b7de5234bb5d0321a50ba96f969`.
- TOML815 bytes, SHA256
  `020c3236ccbf2b74b5bca08401a18aed118564ae2d2636f667ccc8b38542af9c`.

The TOML differs from035 only by `lb2_gpu_timestamps=false`. Preserved previous
console files, build log, source/driver checks and delivery proof are under
`work/analysis/gpu-timestamps-off/`.

Hardware run038 confirms the startup disabled message and zero GPU sample lines.
The first six steady profile windows give3.902279 FPS against3.892705 in035 and
3.897586 in037. The later two windows are5.632 and5.589 FPS. The user noticed no
change. Disabling timestamp commands therefore has no demonstrated sustained
benefit in this workload. No query/queue failure appears and the old crash log
remains18875 bytes. Raw038 logs and matched-window data are preserved in this
experiment's work directory. Keep timestamp queries disabled in the next control
to compare a single logging setting against038.

## Follow-up observation, not changed in this control

Run037 emits2569 native-pressure warning lines;035 emits2550. The pinned channel
code logs the first pressure attempt at warning level and recovery at info level.
The SDK's default synchronous logger flushes its sinks at warning level. These
expected-pressure warnings therefore reach a synchronous flush path on the GPU
submission thread. This is a concrete additional source of observer overhead to
isolate if the timestamp-off result does not explain the remaining stalls. It does
not establish how much wall time the flush costs. Neither driver logging nor log
settings were changed in038. Its log contains2391 such warnings. The next control
uses the existing per-category setting to suppress GPU warning/info messages while
retaining GPU errors and all core/native counters. See `switch-gpu-log-control.md`.
