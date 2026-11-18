# Combine guarded empty-point elision with faster transfers

Run034 confirms a practical improvement from transfer source-pitch specialization.
The next trial enables existing `lb2_elide_no_effect_points` on the same NRO. This
had been tested alone earlier; this run measures its combination with the current
dump and transfer optimizations. No rendering implementation or driver is changed.

Eligibility is restricted to the known constant-position vertex program and
absent/known pixel program, zero color/depth/stencil writes, no vertex/pixel memory
exports, no active occlusion query, and the normal implicit point draw path. The
existing93-check side-effect/eligibility regression passes. Current SDK integration
still constructs these guards before calling the helper.

Only the TOML changed, adding `lb2_elide_no_effect_points=true`. The same NRO was
fully read back and verified, and was not rewritten. Its SHA256 remains
`459d59e7cb6d4f2aee2c61989164fb87273b8ea75a796acdbcee231ce2e35be7`.
New TOML:717 bytes, SHA256
`f1fb0b9380791bde54c83905a0b78f1b3fc61c21cefa953d781236a485d72ba4`.
All prior runtime settings remain unchanged. Original files were preserved; the
new TOML was silently replaced and fully read back. Proof and rollback are in
`work/analysis/combined-no-effect-points/`.

Hardware result pending. Repeat the opening cutscene for60 seconds, exit and
enable MTP. Compare FPS, eligible/elided counts, queue waits and new image/audio
defects; retain the late034 stall as an unresolved stability observation.

## Run035 result and idle attract playback

The user reports similar or slightly better FPS, with multi-second visual pauses
remaining during the beginning and most of the menu flight. The first six steady
reports average3.893 FPS versus3.802 in034, a small change. All eligible empty
points are actually elided (for example47373 eligible/elided calls in one steady
window). No new crash or query/no-space failure was captured. Visual pauses are
not considered solved.

The same log transitions to29.84-30.01 FPS in the idle attract sequence. At that
point there are seven real guest draws per frame, zero memexport draws, roughly
6.41MB of shared-memory uploads per frame and minimal queue-submit CPU time. Its
guest shaders are simple screen-space draws; shader3C2B46393007AE79 fetches three
single-channel textures and combines them into color, consistent with YUV video
playback. This is an inference about presentation type, not a verified movie
filename. It does not demonstrate that the opening3D workload is inexpensive.

Artifacts: `work/analysis/combined-no-effect-points/hardware/`, including the full
log and `windows.json`. Retain the point flag alongside measured pitch fixes.
