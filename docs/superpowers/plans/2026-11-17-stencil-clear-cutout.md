# Guarded stencil-only400x240 overwrite experiment

Selection after Astra directions measured:046 full coverage64CB, RT960 sequence
173.697ms vs other1341.133ms within selected spans (not frame percentages);
memexport allocation/release only2.33ms/s. Next confirmed redundant workload:
stencil restored immediately before exact400x240 REPLACE draw. Trace180 has4
candidates,280 has10, all one00205EB6→00305EB6 transfer [1718,2048),440x240.
Eight stencil bit draws remain but their area falls from440x240 to40x240 each.

Architecture: extend existing per-draw proof with a separate default-OFF
lb2_stencil_clear_cutout flag and strict stencil-only kind. Reuse actual existing
VS interpreter exports; no extra shader execution. Keep full480 proof unchanged.
Require exact27dword VS, PSnull, DepthOnly5, depth8701, exactfront ref0/1 full
write mask, no query/memexport, integer corners400x240/Z0/W1, scale1, usedRT1,
height240, source/dest/range pinned, no host-depth source and no other RTs.

- [x] RED tests: precise eligibility, disabled flag, changed guards, geometry,
  kind separation, successful commit, skipped/placeholder full stencil replay.
- [x] Extend app proof/transfer mode:0normal,1cutout,2stencil-only replay.
- [x] SDK opt-in: keep j0 depth and original full stencil nativezero; only
  stencil-bit mesh rectangles receive400x240 cutout via actual SDK geometry.
- [x] On missing real guest draw replay full stencil, skip j0/host-depth store;
  preserve original source/range to scope exit. Require no stencil-export ext.
- [x] Actual SDK rectangle oracle + sample-value simulation: depth/right border
  preserved for ref0/1; fallback restores all original stencil. Trace replay
  must admit exactly4/10 candidates; baseline full clear rules still5/6.
- [x] Review, required regressions/ARM64 build, incremental SDK patch.
- [x] Install with both GPU query modes OFF and accepted044 settings plus one
  stencil flag; backup046 files/full readback. Ask opening flight047 and image.

No promise of FPS gain: draw count remains8, only covered fragment area changes.
This is a narrow overwritten-aspect cutout, not general lazy stencil validity.

Installed/readback verified; hardware047 performance and image pending. Review
confirmed async discard placeholders cannot occur for these PS-null guards;
actual SDK scheduling predicate contract added to required tests. No general
pipeline scheduler/registry changes were needed. All build/regression evidence
and accepted prior046 backups are under work/analysis/stencil-clear-cutout/.
