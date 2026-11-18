# Third-party notices

This project uses or modifies third-party components. Their own copyright notices
and license terms continue to apply; the project license does not replace them.

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), by Tom Clay and contributors,
  includes portions derived from Xenia by Ben Vanik and Xenia contributors. The SDK
  is distributed under the BSD 3-Clause license. See its upstream `LICENSE`.
- The Switch SDK is based on the Horizon adaptation in
  [NFSMW-NX](https://github.com/StevensND/nfsmw-nx). Its SDK changes use the SDK's
  BSD 3-Clause license; its other components have their own licenses. See the
  [upstream notices](https://github.com/StevensND/nfsmw-nx/blob/20f8dc122bf0cd95211606ebacd3fda550cb21a8/THIRD_PARTY_NOTICES.md).
- The Switch graphics stack uses Mesa and related components. Their licenses and
  copyright notices apply to the respective files. See the upstream sources and
  the NFSMW-NX notices for details.

Versions are recorded in [toolchain-lock.json](toolchain-lock.json). Dependencies
and game data are obtained separately, not distributed in this repository. This
summary is not a complete license inventory for a compiled binary; review the
licenses, notices, source and relinking requirements of all included components
before distributing builds.
