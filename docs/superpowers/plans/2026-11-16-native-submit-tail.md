# Preserve completion capacity in the Horizon queue

The opening cutscene trial 027 aborted with `channel submit failed: no queue space`.
The same failure exists in timestamp trial 016. The exact pending-entry count was
not logged, so the hardware failure mechanism still needs confirmation.

An extracted-function regression reproduces a separate, concrete driver defect:
public producers can accept command entries without leaving enough capacity for
the completion commands and the possible total-order prepend used by submit.
The accepted packet can then fail submission without a way to flush it.

1. Preserve the existing submit skid, completion and ordering requirements in the
   exec, standalone barrier and client-wait producer preflights. Keep public
   headers, physical FIFO size and ordering unchanged.
2. Exercise actual Mesa producer functions near capacity for both completion
   modes and both ordering modes. Check rejection atomicity and mixed streams.
3. Save a reproducible port patch, rebuild the pinned Mesa archives and the NRO,
   and verify the expected symbols and package.
4. Install the same direct-color experiment with only this driver fix changed,
   with silent replacement and full readback verification. Preserve rollback.
5. Test the same opening cutscene. Treat crash prevention and FPS as separate
   outcomes; the performance objective remains open until measured on Switch.

Baseline: the extracted-function test failed before the change and passed 59044
checks afterward. The console was restored to NRO SHA256
`d6752393104241d1b7b03d078f0e1634413b026fec5e49511c0861edefb876b5`
and the original 278-byte FBO configuration before preparing this retry.

Completed local validation: the single changed ARM64 object was rebuilt with the
pinned Ninja rule. It replaced only its member in copies of the installed Mesa
archives; all other member contents were verified unchanged. The full game build
and package checks passed. A bounded independent review found no blocker. See
`docs/research/switch-native-submit-tail.md` for evidence and the full-target
bindgen environment failure avoided by this controlled single-object rebuild.
