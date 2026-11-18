# Opening flight with GPU log filtering

Runs035/037/038 all average about3.90 FPS over the first six steady windows.
Run038 has timestamp query commands disabled, yet the user and counters show no
improvement. It contains2391 native submission-pressure warnings. These warnings
run through the pinned driver's synchronous Vulkan debug callback into REXGPU_WARN.
The default synchronous spdlog logger flushes at warning level, including the SD
file sink. This creates an observer-overhead hypothesis worth isolating before
changing further GPU rendering behavior.

Use the existing supported per-category configuration:

```toml
[log.levels]
gpu = "error"
```

`ParseCategoryLevelsFromConfig` reads this table before `InitLogging`; the logger
factory applies it, and the global log-level change callback preserves explicitly
configured categories. This suppresses GPU info/warnings, including expected
pressure/recovery messages and pipeline-creation messages. GPU errors, core/native
counters and the separate periodic profiler remain. The Vulkan callback may still
construct its message before filtering, so this control specifically removes sink
recording/flush work rather than all debug-callback overhead. A positive result
would identify GPU logging overhead broadly, without proving warning flushes alone
account for the improvement.

No executable, driver, shader, rendering or timing setting changed from038. TOML
structure validation confirms the log table is the only semantic difference.
No rebuild is needed. Before installation, the full current NRO and old config
were read from the console, hash-verified and preserved. The new config was copied
without dialogs, then fully readback verified.

- Unchanged NRO70383813 bytes, SHA256
  `3b41891062ab6280abd3f080b8b659da87301b7de5234bb5d0321a50ba96f969`.
- New TOML947 bytes, SHA256
  `2413bb547a67edca572502835721ae7d67bea5fc95481348b3296cfe3f78121d`.

Evidence and rollback files: `work/analysis/gpu-log-control/`.

Hardware run039 confirms zero GPU warning/info lines, no GPU errors, retained
core/native reports and continuing counted native retries. The first six steady
windows give4.175571 FPS versus3.902279 in038 (about7% higher). Later windows are
5.644 and5.599 FPS, close to038's5.632 and5.589. The user noticed1–3 FPS more during
the menu flight. Retain filtering as a useful reduction in observer overhead;
the broad acceptable-FPS goal is still unresolved.

Native recovery/submit still consumes about8 seconds per10-second window in the
heavy section. The lack of warning lines does not mean pressure disappeared.
The old crash log is still18875 bytes. GPU timing stays off. The next narrow
shader trial retains this exact configuration and changes only redundant address
reconstruction for eligible same-origin stencil transfers.
