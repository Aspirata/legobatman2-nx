# Split dense255 stencil writes from residual bit writes

Measured hot ColorToStencilBit group0x40102 uses2x RGBA8 source and4x depth/stencil
destination. A nonzero bit currently causes a masked stencil write in each of
eight passes. Hypothesis: writing byte255 once with full mask, then discarding
those samples in the residual eight passes, can reduce masked ROP work when255
is frequent. It can add overhead when255 is rare; performance requires Switch
measurement.

Keep exact source addressing/packing, sort/merge, geometry, depth/color state and
sample selection. Add bulk/residual shader flags in unused cache bits31/30. Pitch
fallback must remove only bits20..29 and preserve these mode flags. Compile both
variants before recording any split draw; failure returns to the original eight
passes. Bulk explicitly pushes mask255 and sets writeMask255, then binds residual
and restores the original1<<k mask before each bit draw.

Enable only this measured key, a startup flag defaulting false, and a guarded
query-free IssueDraw scope. Scope begins after the early copy return; IssueCopy
explicitly forbids nested splitting. RAII restores prior thread-local state.
Public SDK headers stay unchanged. Guest occlusion queries retain original draws.

Validate cache/scope/eligibility and real4xMSAA stencil rendering, all256 values
per sample, scissor/canaries, two initial values and depth preservation. Review
the actual integration, build/package, preserve035 rollback and install silently
with full readback. Repeat the same opening cutscene and inspect actual selected
pipeline modes, FPS, pauses and image defects.
