# Horizon submit completion capacity

## Observed failure

Opening cutscene run 027 used the opt-in direct-color resolve. It reached 2488
direct dispatches in its first report, then aborted during the opening cutscene.
The final driver error was `channel submit failed: no queue space
(VK_ERROR_OUT_OF_DEVICE_MEMORY)`, followed by failed Vulkan command submission.
Run 016 (the earlier timestamp experiment) contains the same error. Neither log
records the exact pending entry count at the failing reserve operation, so this
does not prove which reserve failed on hardware.

The user reported a crash at the start of the cutscene. Immediately afterward,
the previous stable NRO and 278-byte FBO config were restored and fully read back.
The original low FPS remains unresolved.

## Reproduced driver defect and fix

The pinned Mesa Horizon channel has a 2048-entry physical queue and retains a
four-entry final skid. Submit additionally needs one fence entry, a second entry
when mapped completion is active, and possibly a prepended total-order wait.
The ordinary exec path previously admitted entries without preserving the
completion tail. A later submit could consequently reject an already accepted
packet as full. Standalone barriers and client waits had the same missing budget.

`port/mesa-horizon-submit-tail-reservation.patch` adds completion and possible
prepend headroom to these three public producer preflights. The wait check occurs
before wait-slice allocation. Submit's final skid, fence/report commands, physical
queue size and ordering are unchanged. The atomic exec-submit outer preflight
already reserves the full packet; the stronger inner checks remain implied by it.

`tools/test-switch-native-submit-capacity.py` extracts the actual Mesa producer
functions and compiles them with deterministic native append/wait-slice stubs.
It failed before the fix and passes 59044 checks after it. It covers mapped and
ordinary completion, order on/off, cache acquire, near-capacity batches, incomplete
chains, standalone barriers/waits, rejection atomicity and mixed producer streams.
It models the final submit admission; it does not execute native GPU submission.
An independent read-only review found no blocking issue or additional public
producer in the channel file.

## Build and controlled retry

The full Mesa target attempted to regenerate unrelated Rust bindings and stopped
because the standalone invocation lacked its bindgen environment. No library from
that interrupted build was installed. Instead, the pinned Ninja target for
`nouveau_horizon_channel.c.o` was successfully rebuilt alone. Its ARM64 ELF object
replaced the corresponding member of copies of the previously installed archives.
Parsed archive verification confirms that all other 1864 members of libvulkan
and all other five members of libnouveau_horizon are byte-identical to the prior
libraries. Only the verified copies were installed in the local SDK.

Evidence: `work/analysis/native-submit-tail/archive-verification.json`,
`channel-build.log`, `game-build.log` and `linked-exec-disassembly.txt`.
The full Switch build, its regression checks and package verification passed.
Guest recompilation was not needed. The patched reserve calculation is present
in the linked executable's exec function.

Retry NRO: 70351045 bytes, SHA256
`4d8ab1edc2e84e1662a9d4658d381dfcad7435c508df92aeae5a78418a125bb6`.
It uses the same 388-byte opt-in direct-color config as run 027. Sparse fence
sampling, timestamps, point elision and stencil-copy trials remain disabled.
Successful local validation is not a Switch runtime or FPS result.

Next measurement: the same automatic opening cutscene for 60 seconds, then exit
and reconnect MTP. Compare crash occurrence, hangs, FPS and new visual defects.
Keep crash prevention and the unresolved performance problem as separate results.

## Hardware retry: runs 028-030

The user reported no crash and no stability improvement. Three new logs were
captured silently in `work/analysis/native-submit-tail/hardware/`; the active
388-byte configuration still enables direct color resolve. None of these logs
contains the no-space error. The crash log remains at its previous 18875 bytes.

After excluding the first report of each run, reported frame rates are 1.60,
3.02 and 3.32 FPS respectively (different run lengths and scene positions prevent
strict comparison). Run 030 covers about 87 seconds and eight reports. Direct
resolve is active throughout, including 709-1428 eligible dispatches per later
interval, but there is no clear practical performance improvement. Native
submission recovery and queue submission still occupy much of each interval.

This supports retaining the queue correctness fix but does not establish direct
resolve as an effective FPS fix. The next experiment repeats the existing sparse
GPU timestamp probe on the corrected queue. The earlier run016 failure had the
same no-space signature; the query implementation itself is still unvalidated
on Switch after this fix. Renderer/config and fixed Mesa libraries remain
unchanged for the timing retry.
