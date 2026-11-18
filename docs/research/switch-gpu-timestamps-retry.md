# Sparse GPU timing retry after queue correction

The prior timestamp experiment (run016) aborted with the same no-space error
seen in direct-color run027. A reproducible queue tail-reservation defect has
since been corrected, and runs028-030 no longer show that error. Their low FPS
remains. The purpose of this retry is to measure GPU operations rather than
continue inferring their cost from CPU waits, draw counts or pixel area.

The existing timestamp recorder is reused without changing its algorithm.
The three implementation-file SDK hooks were rebased around the newer title
hooks. Public SDK headers and fixed Mesa archives remain unchanged. The build
script has an explicit `-GpuTimestamps` switch; its ordinary value is OFF, and
CMake still rejects mismatches between the build option and applied SDK hooks.

The recorder checks timestamp support, uses bounded 128-query pages (at most64),
resets pages before the deferred command stream begins, and samples operations
with BOTTOM-to-BOTTOM markers. Page results are read only after the existing
submission fence completes, with availability and without QUERY_RESULT_WAIT.
Pending or unavailable pages are not reused. Command/query failures disable
sampling; shutdown releases pools after the existing queue drain.

Local recorder regression passes 1002 checks. The full build and package checks
pass. SDK patch reverse-apply validation passes. These do not verify NVK query
hardware behavior. GPU samples are intrusive and overlapping; their totals must
not be summed or extrapolated into frame cost. No FPS improvement is claimed.

NRO: 70371525 bytes, SHA256
`466e4900544aa1d80b1349cf629810d976d222cf5858a327cc98310983f4d8e0`.
The same direct-color/FBO configuration as run030 is used, with sparse CPU fence
drains and other experimental rendering flags still off. The previous NRO and
configuration are saved under `work/analysis/gpu-timestamps-retry/`.

Evidence and installation verification are in that analysis directory. Hardware
test required: the same opening cutscene for60 seconds, exit and enable MTP;
inspect errors and query availability before interpreting the samples.

Installation completed with no-UI replacement and full readback equality for
both files. The prior console NRO/config were also read back and verified before
replacement. Proof: `work/analysis/gpu-timestamps-retry/delivery/verification.json`.
An independent bounded review found no mandatory correction before the test;
unusual SDK drain failures at teardown remain an inherited limitation. Hardware
results are pending.

## Run031: queries work, operation names still needed

Run031 lasts about96 seconds. Timestamp support reports64 valid bits and1ns
period. Nine GPU reports contain available results, zero skipped buffers and no
query/no-space failure. The crash file remains18875 bytes. The user reports the
same behavior, with nearly5 FPS at the end of the opening menu camera flight.
Profile intervals after startup show3.19,3.10,3.38,2.85,2.39,3.59,4.54 and4.50 FPS.

Parsed printed samples are saved in
`work/analysis/gpu-timestamps-retry/hardware/gpu-samples.json`. The most costly
individual printed groups are largely unnamed internal pipelines. For example,
one compute handle has122 samples averaging about2006us and another122 samples
averaging about1071us; an internal draw group has120 samples averaging771us.
These are intrusive selected samples, with different sampling rates by operation
kind and a top16 output limit. They are not a breakdown of total frame time.

The next diagnostic adds bounded creation-time names and keys for internal
pipelines. It preserves the actual query algorithm, rendering/config and fixed
Mesa archives, to distinguish EDRAM dumping, texture loading, ownership transfer,
depth storing, resolves and swap processing before choosing an optimization.

Named diagnostic NRO:70375621 bytes, SHA256
`6905c03bd90c71ec6b7c6b82094b1acf05906615a40510cc6d7def004d9852be`.
The metadata callback emits at most2048 records and performs no GPU operation.
Its reference is weak so ordinary title builds can omit the recorder. SDK call
sites are Switch-only. Source and NRO rollback for031 are preserved under
`work/analysis/gpu-timestamps-named/`; local build, existing recorder regression,
archive equality and reverse-apply checks pass.

Independent bounded review found no blocker in metadata-only callbacks. The named
diagnostic was installed with no-UI replacement; both previous files were preserved
and both new files fully read back and verified. Proof:
`work/analysis/gpu-timestamps-named/delivery/verification.json`.
