# Narrow depth/stencil draw-clear cutout experiment

Evidence: run042 complete frames180/280. Both estimate clear heights precisely;
several PS-null rectangle clears restore all old depth/stencil immediately before
ALWAYS-writing both aspects. Test only the full480x480 depth key003062D0.

Keep the original draw and ownership updates. Reuse ChangeOwnership's existing
cutout solely after exact state/program guards and three actual CPU-interpreted
position exports prove all samples of that rectangle will be overwritten. No
extra shader interpretation, MSAA change, shader modification or native clear.

- [x] Add failing host tests for guards, exact positions, scope lifetime and the
  actual SDK GetRangeRectangles oracle over all720-tile range partitions.
- [x] Add bounded per-draw context and exact27-dword program check; collect
  positions during the existing estimator pass; refuse unsupported cases.
- [x] Prove480x480 cutout coverage only for hostRT depth key003062D0, height480, scale1.
- [x] Review sample coverage, both aspect writes, predicates and pipeline-failure
  behavior before enabling; preserve original behavior on rejected inputs.
- [x] Build with regressions; disable causal file capture; install/read back one
  optional runtime control and retain run039/042 rollback files.
- [x] Record finite trace evidence, test results and hardware comparison protocol.

This is an experiment, not a claim of improved FPS or validated visual output.

Ruling: preserve ChangeOwnership and complete original transfer vectors. Defer Vulkan execution only when real SDK cutout geometry proves every transfer is overwritten; replay on every non-emitting exit, including async placeholders. Public SDK headers remain byte-identical. Alpha-test clears remain rejected. Actual-frame replay admits two clears and45 transfer draws/frame.

Completed: guard/scope/replay tests,259560 SDK partitions, two actual frame replays, independent review, ARM64 build/regressions and full console readback. Hardware043 and any FPS/visual acceptance remain pending.

