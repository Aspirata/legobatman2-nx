# Isolate GPU log recording overhead

Evidence:038 actually disabled GPU queries but remained at3.902 FPS, and emitted
2391 native-pressure warnings. Trace the pinned channel warning through the Vulkan
callback, GPU logger category and default warning-level sink flush. Use only the
existing `[log.levels] gpu="error"` setting; retain errors/core/native counters.

Validate the TOML semantic difference against038, preserve/readback-check current
console files, silently replace only the config, then readback-check it. Keep NRO,
renderer, drivers and all other settings identical. No new tests or rebuild for
this reversible config-only control. Obtain the same opening60-second hardware
run and compare matched frames/time plus native retries. A positive result isolates
logging broadly; warning flush cost alone is not separately established.
