# Same-origin stencil address specialization Implementation Plan

> **For agentic workers:** Use executing-plans for the integration; independent GPU validation and bounded review run in parallel. No commits requested.

**Status:** Paused at the user's explicit request on2026-11-16. The installed trial
produced a report of possible new artifacts; FPS/stall impact and attribution are
unconfirmed. Only Markdown was updated after the stop request. Do not continue
implementation, collect logs, install or roll back until the user resumes work.
Current console still has the trial enabled. See the [hardware report and preserved
baseline](../../research/switch-transfer-same-origin.md#user-report-and-pause).

**Goal:** Remove redundant address reconstruction in the observed0x40102 transfer, preserving all eight stencil passes and their results.

**Architecture:** For2x RGBA8→4x D24S8 at scale1, equal EDRAM base and tile pitch, and rectangles before the2048-tile wrap, the source pixel is `x*2+(sample&1)+(x%40<20?40:-40), y`. Keep the original source sample remap, fetch, packing, discard and draw protocol. Private key bit30 distinguishes this exact shader variant; removing source-pitch specialization must preserve bit30. All ineligible cases retain the original path.

**Tech Stack:** C++23, SPIR-V builder, Vulkan, current pinned SDK; public SDK headers unchanged.

**Spec:** This document; derivation from SDK GetTransferShader and observed039 matching base/pitch pairs. No scratch-buffer or render-pass changes.

## Global constraints

- Retain039 GPU log filtering and all proven optimizations; GPU timestamps remain runtime OFF.
- Only BaseKey0x40102, scale1, matching base/pitch, source/dest bounds and non-wrapping rectangles.
- Validate every rectangle in a merged transfer before selecting bit30, including cutouts.
- Preserve generic fallback identity, descriptor layouts, source sample remap, eight stencil writes and rendering order.
- No public SDK header changes, guest rebuild, driver changes or interactive MTP dialogs.

## Review focus

- Edge pixels at20/40-column boundaries and all four destination samples.
- Last partial EDRAM tile row and wrapped rectangles must fall back.
- Disabled/other-format/different-origin requests must remain generic.
- Cache fallback preserves the private addressing mode bit.
- Actual GPU-produced coordinates match the original integer algorithm.

## Task1: Eligibility and actual shader arithmetic

- [x] Write CPU eligibility/bounds/key tests; observe a meaningful failure on the stub.
- [x] Add `lb2_transfer_same_origin.h` (guard/key rules) and `transfer_same_origin_shader.cpp`/header (production SPIR-V coordinate helper).
- [x] Run CPU tests and independent real-GPU helper comparisons against the original tiled formula, with SPIR-V validation.

## Task2: Integrate and deliver a controlled trial

- [x] Back up RDR source. Add init-only flag defaultfalse and select bit30 only after checking all merged rectangles. Change cache fallback to RemovePitch.
- [x] Override only final source pixel coordinates for this variant; leave original source sample index unchanged.
- [x] Add title helper source and build regression; persist SDK patch.
- [x] Bounded independent review, full Switch build/package, source/driver/header checks.
- [x] Silently install/readback-check NRO and039 config plus the new flag. Request the same60-second opening test; make no FPS claim until hardware evidence.
- [x] Receive user feedback: possible new artifacts; no FPS/stall result supplied.
- [ ] After explicit resumption: capture the new logs, verify activation and compare matched039 windows/stalls/artifacts. Trial remains unvalidated; no rollback was performed during the pause.
