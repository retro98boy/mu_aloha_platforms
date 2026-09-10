# Elden non-Core MU_BASECORE overrides

These complete component copies come from MU_BASECORE commit
`0f519eb579bc6bc0b9ad10170cac2bfc6299a6a1`, compared with base
`762a65dad7`. Source text is preserved apart from line-ending and trailing
blank-line normalization and removal of the RNG compatibility trace guards.
Original copyright and SPDX notices are retained;
`License.txt` is the upstream BSD-2-Clause-Patent license.

- `MdeModulePkg/Universal/Console/GraphicsConsoleDxe`: scaled glyph rendering
  and cursor; the Elden DSC keeps `ELDEN_CONSOLE_SCALE=2`.
- `MdeModulePkg/Universal/Disk/PartitionDxe`: no GPT repair writes and stricter
  SD GPT validation; the Elden DSC keeps `ELDEN_PARTITION_NO_REPAIR`.
- `MdePkg/Library/DxeRngLib`: compatibility retry for the stock RNG protocol
  rejecting a NULL algorithm list, with size validation. This compatibility
  behavior is unconditional in the Elden-only library and needs no trace flag.
- `MdeModulePkg/Library/FrameBufferBltLib`: reverse row copies start at
  `(Height - 1) * LineStride`.

PakalaCommon.dsc.inc selects these copies only for `lenovo-elden`; Pakala.fdf
uses the same local driver INFs. Other devices retain the upstream bindings.
Module GUIDs and package dependencies remain unchanged. Core migration is
maintained separately.

Host regression tests use the local library sources. The RNG test compiles
without trace flags:

```sh
python3 custom-build/test-elden-gop-copy.py
python3 custom-build/test-elden-rng-query.py
```
