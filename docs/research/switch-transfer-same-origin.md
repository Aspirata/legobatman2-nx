# Direct coordinates for same-origin stencil transfers

**Update, 2026-11-16:** the user resumed investigation using Astra's second
opinion. The console files and run040 were collected and the potentially affected
trial was replaced with the exact run039 NRO plus a separate extent-estimation
control. See [current experiment](switch-clear-extent-scissor.md). The pause and
console-state statements below describe the earlier handoff, not the current
installation. No visual acceptance of the same-origin trial has been established.

**Status — paused at the user's request,2026-11-16.** After testing the installed
same-origin trial, the user reported: «Вроде добавились новые артефакты». Treat
this as a possible visual regression, not a confirmed attribution. This trial is
not validated for normal use. No FPS or long-stall result was supplied for it.

Run039 retains heavy native submission waits despite a7% average improvement from
GPU log filtering. The census includes repeated0x40102 transfers with source
`000859E4`→destination`003059E4` and`00085BC8`→`00305BC8`:2x RGBA8 to4x depth,
matching EDRAM base and tile pitch. These do not need the original linear tile
offset/wrap/division to reconstruct their source pixel.

For scale1, destination tiles are40x8 pixels and source tiles80x8 pixels. The
original same-bpp sample transform doubles X and adds the horizontal destination
sample bit; color/depth conversion swaps the two40-sample columns. With equal
origin/pitch and no2048-tile wrap, the resulting source coordinates are exactly:

```
sx = 2*x + (destination_sample & 1) + (x%40 < 20 ? 40 : -40)
sy = y
```

The original source sample remap is unchanged (native2x or emulated0/3). Fetch,
rounding, discard, masks, layouts, barriers, eight draws, draw order and query
behavior remain original. This avoids the scratch-buffer/compute alternative,
which would add resources and a render-pass break without removing stencil writes.

Init-only `lb2_transfer_same_origin` defaults false. Exact base key0x40102,
standard sample locations, matching valid base/pitch and scale1 are required.
Every rectangle in a merged transfer must pass image bounds and a64bit maximum
relative tile-index check `<2048`. The width guard requires a full destination
pitch row and at least twice that source width, protecting the nonmonotonic
column swap. Ineligible rectangles preserve original addressing.

Private key bit30 marks the shader; it is canonicalized without source-pitch bits
because the direct shader no longer depends on pitch. The cache's RemovePitch
fallback preserves addressing mode. Failed optional pipeline creation retries
the original pitch/generic key before any transfer draw. Rejected stencil split
also used bit30, so CMake explicitly rejects combining its SDK hooks with this
trial. SDK public headers remain unchanged.

Validation so far:

- Meaningful RED before implementing eligibility;21493 CPU bounds/eligibility/key
  checks now pass.
- Actual production SPIR-V helper ran on RTX5060Ti:98304 coordinate pairs across
  12 pitches1..1023, all four samples, first/last tiles and column boundaries,
  compared against an independent original tiled-address reference.196608 scalar
  outputs match; SPIR-V validates for Vulkan1.2.
- Independent algebra review additionally checked163648 CPU boundary cases.

Artifacts: `work/analysis/transfer-same-origin/`, GPU validation in
`work/analysis/transfer-same-origin-gpu/`. SDK patch:
`port/rexglue-horizon-transfer-same-origin.patch`.

Full Switch build/regressions/package and final independent review passed. The
optional-pipeline fallback identified during review is included. Isolated reverse
application of this and preceding SDK patches restores the same normalized five
rendering sources as the prior baseline. Public headers and corrected Mesa archive
hashes remain unchanged. Linked ELF contains the new helper/cvar and no rejected
stencil-split helper/scope. Its CMake option is OFF; GPU timestamp hooks remain
compiled but disabled at startup.

Silently installed and fully readback verified:

- NRO70383813 bytes, SHA256
  `d45e2d4b841ea75fc297cb10eca56d19a74b38dadf6f19e2f013f30b43652f33`.
- TOML1062 bytes, SHA256
  `ff18f5a6e7b3ec5b95a0b5df521809633a74d799032ba9b2df74d2674b88656a`.

The TOML differs semantically from039 only by `lb2_transfer_same_origin=true`;
GPU log filtering, timestamp-OFF and other proven optimizations remain. Previous
console files are preserved in `delivery/before`, with SHA proof in
`delivery/verification.json`.

## User report and pause

The user tested this installed trial and noticed possible new artifacts. The
report does not identify their location or visual form, and does not establish
whether they differ from the older depth-like artifacts present since480p. Do not
merge the two observations or claim the new shader caused the defect conclusively.
The CPU/GPU coordinate-helper checks above do not establish correctness of the
complete rendering integration on Switch.

The user explicitly requested stopping work and recording the information in
Markdown. Only documentation was updated after that request. No new build, log
capture, installation, config change, rollback or hardware test was performed.
The last verified console state therefore remains this potentially affected
trial: NRO`d45e2d4b...`, TOML`ff18f5a6...`, with the source patch applied and runtime
`lb2_transfer_same_origin=true`. Earlier GPU log filtering and proven pitch fixes
remain enabled; timestamp commands remain disabled.

The previous039 baseline is preserved at
`work/analysis/transfer-same-origin/delivery/before/`:

- NRO SHA256`3b41891062ab6280abd3f080b8b659da87301b7de5234bb5d0321a50ba96f969`.
- TOML SHA256`2413bb547a67edca572502835721ae7d67bea5fc95481348b3296cfe3f78121d`.

That rollback has **not** been installed after the pause. Resume only on the
user's instruction. Still unverified: new-run log capture/identity, activation of
canonical key`40040102`, FPS/stall comparison against039 and visual comparison
against the prior baseline. No performance gain or artifact fix is established
for the same-origin trial; the acceptable-FPS objective remains unresolved.
