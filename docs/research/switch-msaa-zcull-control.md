# Independent ZCULL control for multisampled depth

Earlier control023 disabled ZCULL eligibility only on1x depth targets. The measured
hot ownership transfers instead involve2x/4x guest depth targets. This experiment
tests that independent scope after restoring the measured035 stencil behavior.

Startup flag `lb2_msaa_no_zcull_control` defaults false. Inside the existing depth
image branch it requests TRANSFER_DST usage only for guest2x/4x. The pinned NVK
allocator permits ZCULL only for depth attachment plus read-only usage, so that
extra bit excludes the plane through the normal driver rule. There is no sample
count exception in that rule.1x depth and color targets remain as before.

No sample counts, image dimensions, shader/address/packing logic, draw order,
copy commands or driver code are changed. The rejected stencil split is unapplied
and compiled OFF; ELF verification finds neither its helper, TLS scope nor cvar.
CPU command processor source matches its pre-split baseline. Existing pitch fixes,
point elision, direct resolve, GPU timestamps and metadata remain enabled.

Full Switch build/regressions and package checks pass. Independent bounded review
confirms the scope and NVK eligibility rule. Corrected Mesa archives retain their
hashes. Isolated reverse application of the control and prior SDK patches restores
the normalized source baseline. Public SDK headers remain unchanged.

Installed NRO:70375621 bytes, SHA256
`28e94cb6ae1e0db5e49b9a404fed7301a0624aa627af5038ce30102ba5fe4f37`.
Config:818 bytes, SHA256
`cc6865278ce27cbd622e5246a29f1bf6ef025e2aa596b2905224e4b908d8d472`.
The only added config value relative to035 is the new MSAA control flag. Both old
and new files were fully verified by readback; installation used no-UI replacement.
Proof and rollback: `work/analysis/msaa-zcull-control/`.

Hardware run037 completed. Driver stderr confirms absent ZCULL planes on the
MSAA depth targets, while1x depth still receives a plane. The first six steady
profile windows give3.897586 FPS against3.892705 in035; later windows are again
around5.6 FPS. This control has no demonstrated sustained gain. The user noticed
about1 FPS more at the end, without a broad change. Depth artifacts remain after
removing the stencil split, so their attribution to that experiment is unproven;
they remain deferred at the user's request.

No query/queue failure was found in037 and the old crash log is unchanged. Raw
logs and the window comparison are saved under this experiment's work directory.
Restore the normal ZCULL setting for the next control, which measures the proven
renderer with GPU timestamp commands disabled.
