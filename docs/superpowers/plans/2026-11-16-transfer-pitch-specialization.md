# Specialize source pitch in ownership-transfer shaders

Run033 confirms dump specialization activates and reduces selected dump GPU time
per workgroup from about0.303us to0.243us. Its overall FPS improvement is small:
3.646 to3.720 over comparable later reports. Apply the same exact constant-divisor
approach to measured costly ownership transfers as a separate opt-in trial.

Preserve the20 existing TransferShaderKey bits and encode source pitch1..1023 in
unused bits20..29. The dynamic source pitch push remains unchanged. Reuse the
already GPU-validated uint32 quotient/remainder helper; do not change packing,
depth/stencil operations, layouts, barriers or draw counts. Specialize only the
local pipeline request after the original invocation sort/merge to preserve order.
GetTransferPipelines reuses existing variants first, and after128 cache entries
new requests use generic base-key pipelines. Public SDK headers stay unchanged.

Run key layout/identity/fallback tests and the full Switch build; obtain bounded
review of source-pitch binding, generic fallback and unchanged sorting/merging.
Preserve run033 NRO/config. Install silently and verify full readback. Keep the
run033 settings and add only lb2_transfer_pitch_specialization=true. Repeat the
same opening cutscene and compare matching internal operations and FPS.

Local checks, full build/package, bounded independent review and silent install
with full readback have passed. Hardware comparison is pending. Evidence:
`docs/research/switch-transfer-pitch-specialization.md`.
