# EDRAM dump source-pitch specialization trial

Run032 GPU samples now have internal pipeline names. Selected samples include
1x UNORM depth dumping (base key0x40,126 samples, mean1954us),2x RGBA8 color
dumping (base key1,147 samples, mean967us), and ownership transfers. Direct RGBA8
resolve samples are much shorter (552 samples, mean25us). These are intrusive,
selected samples with different sampling rates, not a frame-cost breakdown.
No query/no-space error was recorded. Later profile windows again reach4.5 FPS.
Evidence: `work/analysis/gpu-timestamps-named/hardware/named-gpu-samples.json`.

The dump shader performs uint32 division and modulo of source tile index by a
push-constant pitch. In the pinned Mesa NAK compiler, nir_opt_idiv_const runs
before nir_lower_idiv. The latter lowers dynamic division into a reciprocal,
multiply-high operations and quotient/remainder refinements. The testable
hypothesis is that making the already-known source pitch a shader literal will
reduce address-computation cost. Its FPS effect is not established locally.

The implementation retains the original UDiv/UMod, operation order, data packing,
descriptors, push constants, dispatch dimensions and barriers. A startup flag
`lb2_dump_pitch_specialization` defaults to false. Both dump-key producers encode
the actual RenderTargetKey::GetPitchTiles(), which is also the value written to
the existing source-pitch push field. The original7-bit format/sample/depth key
is preserved; pitch1..1023 occupies unused bits7..16. Invalid/disabled inputs use
the original dynamic-pitch shader. Public SDK headers are unchanged.

GetDumpPipeline first reuses existing cached variants. Once its cache reaches64
entries, it converts new specialized requests to the generic base key. Checking
admission here also covers invocation lists collected before pipeline creation.
The separate direct-resolve preflight alias map keeps the original generic key.
Sorting can change across pitch variants, but dump ownership ranges do not overlap.

Validation passed:

- 524165 cache identity, bit preservation, bounds and generic fallback checks.
- Real Vulkan GPU execution of the actual production helper:168960 uint32
  quotient/remainder pairs across33 dispatches on RTX5060Ti, including the whole
  EDRAM index range, full32-bit boundary/random values,16 pitches and invalid-pitch
  fallback. All33 SPIR-V modules passed Vulkan1.2 validation. This is an arithmetic
  test, not a whole-image or Switch performance test.
- Full Switch build/regressions/package verification; no guest recompilation.
- Independent bounded review found no blocker. Corrected Mesa archives match
  their previous hashes. Isolated reverse application of specialization then
  timestamp patches restores the baseline source (line endings normalized).

Artifacts: `work/analysis/dump-pitch-specialization/` and
`work/analysis/dump-pitch-gpu/`. Port patch:
`port/rexglue-horizon-dump-pitch-specialization.patch`.

Installed trial NRO:70375621 bytes, SHA256
`abfb520f6dffc2ff782535fdfd092a0c851722e3039815eb0d058481f9f6d647`.
Config:491 bytes, SHA256
`f8da3226b50adc04e086aae8124bb3a5fcf68c33e8a3357592a55dcf266b89f9`.
Its only changed config value is the new specialization flag. Renderer, direct
color resolve, timestamps and pipeline-name metadata remain as in032.

Silent installation and full readback of both old and new files succeeded.
Proof: `work/analysis/dump-pitch-specialization/delivery/verification.json`.
Hardware result is pending; repeat the same60-second opening cutscene and compare
matching dump types/pitches/work sizes before judging speed or FPS.

## Run033 hardware result

Specialized dump keys are present (source pitches11,12 and16). No query/no-space
error or new crash record was captured. Across comparable later profile reports,
FPS changes from3.6459 in032 to3.7198 in033, a small practical change. For printed
dump samples, sampled time divided by sampled workgroup count decreases from
0.3034us to0.2432us for1x depth and0.3037us to0.2433us for2x color. These are
selected intrusive samples, with different sizes/pitches mixed in the generic
baseline; they support a kernel improvement but are not a total frame breakdown.
Evidence: `work/analysis/dump-pitch-specialization/hardware/result-summary.json`.

Retain the opt-in dump specialization for the next controlled trial, extending
the same constant-divisor technique to ownership-transfer shaders separately.
