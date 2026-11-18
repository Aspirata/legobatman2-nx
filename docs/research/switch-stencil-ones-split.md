# Full-byte255 stencil split trial

The experiment targets only ColorToStencilBit base key0x40102 (2x RGBA8 to4x
depth/stencil), one of the measured slow transfer groups. The source byte is
computed exactly as before. A bulk shader keeps only lowbyte255 and writes with
reference/writeMask255; eight residual bit passes discard255 and otherwise use
the original bit test. This gives the same stencil OR-update for every possible
source byte and initial destination, without changing depth or color.

The goal is fewer masked ROP writes for dense255 regions. Draw count can rise
from eight to nine; the speed hypothesis depends on actual data, and no Switch
FPS gain has been established for this new split yet.

`lb2_stencil_ones_split` defaults false and is startup-only. Mode flags occupy
unused TransferShaderKey bits30/31; original bits0..19 and pitch20..29 are retained.
Cache-budget fallback removes only pitch, preserving bulk/residual semantics.
Both pipelines must preflight before the first split draw; partial failure uses
the original protocol. Existing sort/merge, rectangles, source packing/addressing,
descriptors, barriers, sample-specific pipelines and guest-state tracking remain.

Guest occlusion counts would change under splitting, so it is restricted to a
thread-local RAII scope for regular IssueDraw with no active host occlusion query.
The scope starts after the early kCopy return, and IssueCopy explicitly installs
a forbidden nested scope. Outside guarded draws it defaults false. Bulk explicitly
pushes mask255 and sets writeMask255; residual restores each original bit mask.
SDK public headers are unchanged.

Local validation passed:

- 18460 eligibility, cache-mode/fallback and nested/thread-local scope checks.
- Real Vulkan rendering of production helper predicates: original8 versus
  bulk+8 residual on4xMSAA D24S8 and D32S8, every byte value per sample, different
  sample values, initial stencil0 and0x12, scissor and outside-region canaries.
  Eight protocol/format/initial combinations compared532224 readback bytes.
  A separate conditional stencil inversion checks that original depth remains
  equal to its clear value. All helper fragment and readback SPIR-V validates for
  Vulkan1.2. The test covers the write protocol, not full SDK source addressing;
  it does not test1x mode, which is outside this pilot eligibility.
- Full Switch build/regressions/package, bounded independent integration review,
  unchanged corrected Mesa archive hashes, and isolated reverse application of
  all four SDK experimental patches back to normalized source baseline.

Artifacts/rollback: `work/analysis/stencil-ones-split/` and GPU results in
`work/analysis/stencil-ones-gpu/`. Installed NRO:70379717 bytes, SHA256
`40ee31c930be7f3d6b5855a78126b8c6288d6fccb215fe60abaabfdad742ca1b`.
Config:827 bytes, SHA256
`baa2526746ece9be30e1273d6a65716a26ffeb0c0460b5b6e769878106182e15`.
The only runtime-config addition to035 is the new split flag. Prior files were
preserved; both new files were silently replaced and fully verified by readback
(`delivery/verification.json`).

Hardware result is pending. The overall acceptable-FPS objective and reported
multi-second visual pauses remain unresolved. Repeat the original60-second
opening cutscene rather than judging the much lighter idle attract playback.

## Run036 result: reject this performance trial

Both bulk/residual pipeline keys are present, so the trial activated. Its first
six steady windows average3.791 FPS versus3.893 in035, with no practical gain.
Selected residual samples for the hot group remain expensive (~636us mean), and
printed bulk samples also cost roughly538-697us. These intrusive samples do not
establish the source-byte histogram or why the ROP hypothesis failed.

The user noticed possible new depth-order artifacts: objects behind other objects
appeared over them. The user explicitly noted these may have existed at spots
previously obscured by freezes, and requested that artifact investigation wait
until FPS work progresses. Record this as a possible regression, not a confirmed
cause or fixed defect.

Because the trial gave no FPS gain, the console was silently restored to the035
NRO459d59e7... and717-byte configf1fb0b93..., both fully read back and verified.
Proof: `work/analysis/stencil-ones-rollback/delivery/verification.json`.
The SDK patch is now unapplied. Normal builds require
`LB2_ENABLE_STENCIL_ONES_SPLIT=OFF`, reject applied split hooks and omit its shader
helper from the title target. Explicit source/build opt-in remains available for
later investigation. Existing CPU/GPU test artifacts and failed NRO are retained.
