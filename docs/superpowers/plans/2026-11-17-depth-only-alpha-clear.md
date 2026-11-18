# Extend the proven full clear to the exact DepthOnly alpha-register case

Run043: verified activation965 committed, no fallback/failures or GPU errors;
4.6139 vs0394.1756 FPS (+10.50%), transfers/frame reduced about11%. User confirms
image unchanged. Preserve this setting and test one additional optional flag.

Source evidence: DepthOnly bypasses the effective PS; hostRT D24Unorm has no
fragment stage. The observed leftover alpha-test state therefore cannot discard.
Admit only optional flag + raw RB_MODECONTROL5 + witnessed RB_COLORCONTROL8700000E/C, with
every existing program, geometry, sample, query and replay guard retained.

- [x] Add failing tests for the exact alpha exception and rejection of other modes.
- [x] Add default-false lb2_depth_clear_depth_only_alpha and scoped State fields.
- [x] Independent source review: alpha bypass valid; require real non-placeholder
  pipeline to commit even if the reloadable async setting has changed.
- [x] Test old OFF replay2/45 unchanged and expanded ON replay5/72,6/81.
- [x] Build/regressions; retain run043 rollback, install/read back one-setting trial.
- [x] Record result and request the same opening-flight hardware comparison.

No renderer alpha behavior changes: only omit imports already proven overwritten
by the existing fragment-free depth/stencil draw. New FPS gain remains unmeasured.

Completed: host guards, old/expanded trace replays, ARM64 build/regressions, independent review and full console readback. Hardware044 and new FPS/visual acceptance remain pending.
