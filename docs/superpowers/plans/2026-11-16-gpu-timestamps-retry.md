# Repeat sparse GPU timing after queue-tail correction

Runs 028-030 did not repeat the no-space crash. Their steady FPS was about
1.60, 3.02 and 3.32 respectively; the last two represent the familiar low-FPS
behavior. Native submission recovery and Vulkan queue submission still dominate
CPU wall time. Direct color resolve is active but did not produce a clear gain.

Run 016's timestamp diagnostic failed with the same no-space error as run 027.
Now that a reproducible missing completion-reservation defect is fixed, repeat
the existing timing probe to determine which GPU work is slow. This is a
diagnostic experiment, not an FPS optimization or proof that queries are safe
on Switch.

1. Capture the three new logs and preserve the current working NRO, config and
   SDK source files. Keep the fixed Mesa archives unchanged.
2. Reapply the existing timestamp hooks to current private SDK implementation
   files, preserving all newer hooks and public headers. Explicitly opt in at
   build time. Keep the same renderer and direct-color configuration as run 030.
3. Run the recorder regression, inspect Vulkan lifecycle and integration, and
   build/package. Use read-only independent review before installation.
4. Install with silent replacement and full readback. Test the same cutscene
   for 60 seconds. Treat GPU operation samples as intrusive and overlapping,
   and inspect validity/errors before using their durations.

Rollback: work/analysis/gpu-timestamps-retry/before.nro and before.toml.
Private SDK source baseline: work/analysis/gpu-timestamps-retry/base/.
