# Measure the current renderer without intrusive GPU timing commands

Run037 confirms MSAA ZCULL planes are excluded, but its first-six-window FPS is
3.898 versus3.893 in035, with later windows again around5.6 FPS. The user reports
an instantaneous improvement near the end, without broad change. Return ZCULL to
its normal state rather than treating the control as an established optimization.

The renderer has run with sparse GPU timestamps since031. Their BOTTOM markers
and resets can alter GPU synchronization. Add an init-only runtime switch in the
existing diagnostic recorder, preserving enabled behavior by default. When false,
initialization must stop before query allocation/reset/write/results and labels
must not allocate per-draw records. CPU observation/counters remain available.

Write a recorder regression that fails on current behavior, implement the switch,
run the full recorder/build/package checks and bounded review, verify unchanged
driver/SDK rendering source, and install silently with readback. Use normal035
settings plus lb2_gpu_timestamps=false; the no-ZCULL control is removed. Test the
same opening60 seconds to establish FPS without the GPU timing commands.
