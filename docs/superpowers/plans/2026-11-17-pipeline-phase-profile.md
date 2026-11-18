# Whole-pipeline measurement after run044

User steering: stop further clear/stencil specializations until the other Astra
directions have been measured. No partial-stencil code is implemented.

Preserve accepted044 renderer (+14.76% vs039, image unchanged). Add one diagnostic
mode: GPU timing of contiguous workload phases in a sparse selection of complete
command buffers; CPU split of the synchronous memexport path. This measures fixed
RT960 guest rendering alongside ownership, dump, resolve, texture, host-depth,
copy and other guest work; it does not infer their costs from pixel counts.

- [x] Add failing phase tests to existing Vulkan recording-backend harness.
- [x] Add opt-in phase mode to existing timestamp recorder; sample1/32 buffers,
  max64 selected/8 pools; only contiguous phase boundaries, no per-draw pairs.
- [x] Read only after existing fence completion plus4 submissions, no WAIT and
  no added queue drains; preserve legacy timestamp mode when phase mode is OFF.
- [x] Attach real render-area width and pipeline role names at deferred execution.
- [x] Split full memexport CPU scopes: allocation, recording, wait, map,
  invalidation, copy, release. Keep operations and synchronization unchanged.
- [x] Build/regressions and review; install/read back one diagnostic setting.
- [x] Document sampled scope limits and request same opening-flight run045.

Do not extrapolate sparse GPU segments to whole-frame percentages. Report known
vs unknown roles and coverage; sample overhead means this run is diagnostic.

Final ARM64 build and required regressions passed; timestamp harness2537 checks.
Review approved after role/coverage corrections. Both installed files fully read
back and SHA256 verified; accepted044 pair saved in preinstall. Hardware run045
is pending; no performance conclusion from the new measurement mode yet.
