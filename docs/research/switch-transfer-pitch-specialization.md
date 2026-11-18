# Ownership-transfer source-pitch trial

Run033 confirms the prior dump specialization works and reduces selected GPU dump
time per workgroup by about20%, while overall FPS changes only slightly. The next
trial applies the same exact uint32 division/modulo specialization to the source
address calculation in ownership-transfer fragment shaders.

`lb2_transfer_pitch_specialization` is startup-only and defaults to false.
The pinned TransferShaderKey uses bits0..19; source pitch1..1023 is encoded in
unused bits20..29 for shader and pipeline cache identity. CPU validation checks
the actual pinned header field layout. SDK public headers remain unchanged.

Only a local pipeline request is specialized, after the original invocation sort
and merge. The original invocation keys, processing order, vertex data, draw
counts, depth/stencil logic, host-depth-source divisor, descriptors, push constants
and barriers are preserved. Literal source pitch is taken from the same source RT
as the bound image and source_pitch push field. The shader reuses the previously
GPU-validated production UDiv/UMod helper without modification.

Existing pipelines are reused first. After128 transfer pipeline cache entries,
new specialized requests fall back to the generic base key before shader lookup,
so both module and pipeline caches agree. The transfer census still records the
requested local key; decode BaseKey for comparisons. Actual selected pipeline
identity comes from the timestamp pipeline-name metadata.

110569 cache identity and fallback checks pass. Full Switch build/regressions and
package verification pass. Independent bounded review found no blocker in layout,
source binding, cache fallback or unchanged ordering. In isolated source copies,
reversing transfer specialization, dump specialization and timestamp patches
restores the normalized baseline. Corrected Mesa archive hashes remain unchanged.

Artifacts and rollback: `work/analysis/transfer-pitch-specialization/`.
Installed NRO:70375621 bytes, SHA256
`459d59e7cb6d4f2aee2c61989164fb87273b8ea75a796acdbcee231ce2e35be7`.
Config:599 bytes, SHA256
`37cf11a238f2b14e5082aff1151996f1416d944a6a8fd9535eabe6c60bab9c3d`.
Its only config change from033 is the new transfer specialization flag. Dump
specialization, direct color resolve, FBO, GPU timestamps and metadata remain.
Both prior files were preserved and both new files silently installed and fully
verified by readback; proof is in `delivery/verification.json` in that directory.

Hardware result pending: repeat the same60-second cutscene, exit and enable MTP.
Compare matching transfer base keys, pitches and work sizes before interpreting
sample durations; do not sum or extrapolate intrusive operation samples.

## Run034 hardware result

The user reports a noticeable1-2 FPS improvement across much of the opening and
the end of the menu camera flight. Logs confirm specialized transfer keys and no
query/no-space abort. The first six comparable post-startup reports average3.135
FPS in033 and3.802 in034 (about21% higher). Later034 windows reach5.03 and5.56 FPS.
Do not compare whole-run averages directly:033 and034 have different lengths.

One frequent ColorToStencilBit group (base key0x40102) has selected sample mean
836us in033 versus603us with source-pitch11 in034; draw geometry/work sizes vary,
so this is evidence of an operation-level improvement, not a full frame breakdown.
Other dumps retain their prior specialized behavior. The crash file remains18875
bytes. A late034 interval includes a3212ms recovery event and2062ms GPU buffer
span; its cause is unclassified, and it is outside the matched first-six windows.
This stall must not be described as solved or silently excluded from stability
assessment. Artifacts: `work/analysis/transfer-pitch-specialization/hardware/`.

Next controlled combination: retain this NRO and both measured specializations,
then enable the already-implemented guarded no-output point elision. Current logs
still show roughly45k eligible calls per10 seconds and zero actually elided calls.
The recorder/query guards and no-output conditions passed their93-check host
regression again before changing the configuration.
