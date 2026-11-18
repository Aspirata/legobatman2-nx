# Reuse a proven zero-stencil generation for one subsequent material draw

047 reduced recorded transfer area4.63% but steady FPS4.7966 vs0444.7918 is only
+0.099%, no established significant gain. User image unchanged. Stop additional
pixel-area cutout extensions; test removing8 stencil draws from a proven-zero source.

Existing exact480x480 full clear003062D0 writes stencil0 at all samples in tiles
[720,1440). In trace180/280 a following003062D0→002062D0 full-range transfer
copies960x960 zero stencil using8 bit draws AFTER an existing full nativezero.
Intervening no-output point calls perform no GPU work. Preserve depth/nativezero;
omit only bit invocations with a short-lived proof tied to the source VkImage.

Use a separate app module with a per-IssueDraw Scope. A real, witnessed full clear
publishes one image token. EVERY VulkanRTUpdate entry takes/clears the token into
that draw's scope; any unrelated material draw consumes it without reuse. Resolve
entry clears global/current proof, RT destructor forgets the exact image. A token
cannot span another material draw or survive object destruction. No lazy pending
image data, owner-model changes, persistent validity map, queue waits or shader edits.

- [x] RED: token absent/disabled, real full clear vs failed draw, one-use source,
  next unrelated material draw, resolve/reset, destructor/handle reuse, exact
  transfer range/keys/geometry/scale/noEXT/host-depth/otherRT guards.
- [x] Implement app Scope/token and integrate full-clear real draw publication.
- [x] SDK Scope and lifecycle hooks; omit stencil-bit invocations only AFTER
  original native stencilzero, keeping j0, barriers, source and transfer list.
- [x] Source-backed trace replay and reference sample equivalence; expected
  eligible groups from proof, not assumed zero from depth-only register state.
- [x] Review, regressions, ARM64 build, incremental SDK patch.
- [x] Install on top accepted047 with one new default-OFF flag, queriesOFF,
  full readback, then same flight048 and image check.

This is bounded stencil-generation reuse from Astra3, not generic lazy stencil
materialization. FPS impact unverified; strict misses retain original8 passes.

Intentional dependency: zero reuse alone does not enable full-clear recognition
or old import cutouts; it requires the existing full-cutout proof enabled in047.
Active occlusion/visibility query disables this draw's zero-reuse Scope.

Final ARM64 build/regressions/review passed. Actual causal replay5/6 groups,
40/48 stencil draws avoided. Installed pair fully readback verified; prior047
preserved. Hardware048 and image/performance results pending.
