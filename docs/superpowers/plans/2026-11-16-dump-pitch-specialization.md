# Specialize exact EDRAM dump division by source pitch

Run032 identifies the costly internal kernels as EDRAM depth/color dumping and
ownership transfers. Direct RGBA8 resolve is much cheaper in selected samples.
Dump shaders divide and take modulo by a source-pitch push constant. The pinned
NAK optimizer performs exact constant-divisor optimization before generic uint32
division lowering, so compiling the known source pitch into the dump shader is a
specific performance hypothesis. This does not establish its impact on FPS yet.

Keep original UDiv/UMod, source data, packing, descriptors, dispatches and barriers.
Encode source pitch in unused private cache-key bits, preserving the SDK header.
Both key producers use the render-target pitch also written to push constants.
Make this startup opt-in, default false. Admit new variants only while the cache
has fewer than64 entries; afterward retain existing variants or use the original
dynamic shader. Enforce admission in GetDumpPipeline, after invocation gathering.

Validate all base-key/pitch identities and fallback boundaries, the actual shader
helper on real Vulkan with full uint32 inputs, host regressions, build/package,
and a bounded independent review. Preserve run032 rollback. Install with no-UI
replacement and full readback; compare the same opening cutscene and GPU samples.

Local validation, bounded review, full build/package and silent installation with
full readback have passed. Real Vulkan arithmetic test:168960 q/r pairs,33 valid
SPIR-V modules. Hardware comparison remains pending. See
`docs/research/switch-dump-pitch-specialization.md` for artifacts and hashes.
