# Test ZCULL specifically on multisampled depth surfaces

Run036 stencil full-byte split activated but gave no FPS gain and possibly new
depth-order artifacts. Preserve that report for later investigation, roll back to
the measured035 NRO/config, and remove the rejected split from normal builds.

Earlier no-ZCULL control023 changed only1x depth image usage. The hot measured
transfer paths instead use2x/4x guest depth surfaces. Test that independent scope:
add a startup flag default false that requests TRANSFER_DST usage only for2x/4x
depth targets. The pinned NVK image allocator then excludes those images from
ZCULL using its existing conservative eligibility rule. Keep1x targets, color
targets, shaders, draw order, copies, sample/address logic and other flags intact.

Save the small port patch, review the scope and driver eligibility, build/package
with rejected stencil split OFF, verify unchanged Mesa archives and no split
symbols/hooks, install silently with full readback, then repeat the original
opening cutscene. Confirm the intended ZCULL state from driver logs before
interpreting FPS. This is a control experiment, not an established optimization.

Source/build rollback, independent review, local checks, full build/package and
silent installation with complete readback have passed. Hardware result pending.
Evidence: `docs/research/switch-msaa-zcull-control.md`.
