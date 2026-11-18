# Complete RT960 groups after diagnostic045 truncation

Measured045: memexport local allocation/release only3.50ms/s vs waiting94.75ms/s
in the six steady windows. Pool allocation is deprioritized. Detailed GPU phase
mode captured53 buffers but truncated2618 segments; RT960 covered467 operations
vs179690 observed. Do not rank RT960 cost from these biased prefix samples.

Choose a minimal opt-in measurement correction: retain045 detailed mode when
new init-only lb2_gpu_phase_coarse=false. When true, collapse GPU timing into
rt960_sequence vs other_gpu continuous groups, retaining the original per-role
observed census. Switch sequences only at known guest draws, by their actual
render-area width; internal compute/copy/graphics helpers do not switch them.
This is an envelope including intervening helpers, not pure guest draw cost.
Entry transfers can fall outside and following helpers can fall inside; no
precise causal association is inferred. Whole-buffer spans and safety remain.
No renderer, allocation, synchronization, surface sizes or shaders change.

- [x] RED: hundreds of non960 phase changes followed by a late RT960 group must
  yield only3 group pairs plus buffer pair; the full late group must be captured.
- [x] Add coarse flag and grouping, selected/recorded operation counters.
- [x] Verify selected vs recorded counts expose incomplete groups, including
  a deliberately overflowing alternating960/rest stream. Preserve045/legacy tests.
- [x] Review and build; record045 result and its limits in documentation.
- [x] Install/read back against proven045 identities; request046 opening flight90s.
- [x] Hardware046 capture and coverage analysis after the user runs the flight.

GPU conclusions require zero truncation and actual guest_rt960 selected_operations
equal to recorded_operations. Its census sampled_operations remains zero. Both
coarse roles require selected=recorded=sampled_operations once buffers_read=selected.
Transfer/resolve association with960 surfaces still needs causal trace; other_gpu
is an inclusive comparison group, not an attribution to any one internal role.

ARM64 build and required regressions passed, timestamp harness2962 checks.
Review approved. Full preinstall045 backup and new file readback SHA256 verified;
new046 mode is installed, hardware run still pending. Materials and identities:
work/analysis/rt960-coarse-profile/{build.log,build-verification.json,verification.json}.

Hardware046 complete:64/64 buffers read, zero truncation, selected=recorded
actual9608742; both coarse roles selected=recorded=sampled. RT960 sequences
173.697ms vs1341.133ms other,11.47% of selected CB spans only. Reduce priority
of960 resizing. Next confirmed candidate is partial stencil overwrite cutout.
