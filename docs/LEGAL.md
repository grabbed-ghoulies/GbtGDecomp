# Legal

GbtGDecomp is original software written from reverse-engineering a legally obtained copy of *Grabbed by the Ghoulies*.

## What this project ships

- Source code, build files, and documentation authored by this project
- File **checksums and sizes** used to verify a user-supplied dump
- Format notes derived from inspecting the Xbox executable and asset containers

## What this project does not ship

- Any file from the Xbox disc (`default.xbe`, `.bnl`, hashed assets, audio, Bink movies, promo trailers)
- Microsoft Xbox Development Kit headers or libraries
- RAD Game Tools Bink SDK, or a translation of the XBE `BINK` sections
- Copies of third-party reverse-engineering trees that have no license

End users must supply their own retail files. Tools accept `--game-dir` (an extracted disc folder). ISO extraction is a later convenience, not a distribution channel.

## Bink movies

In-game clips in `movie/` are Bink (`BIKi`). `gbtg.exe` decodes them with FFmpeg's LGPL Bink decoder, fetched at build time and copied beside the executable. The RAD SDK stays out of the tree, and the XBE `BINK*` sections are not translated. FFmpeg's license is LGPL; the shared libraries next to `gbtg.exe` are that build. Linux builds compile the same FFmpeg release from unmodified source with only the Bink demuxer and decoders enabled, and ship it as shared libraries beside `gbtg`; the Linux package carries the LGPL text and the other libraries' licences in `licenses/`.

## Third-party research

Public write-ups of Rare AID hashing, CAFF/BNL layout, and Xbox texture tiling were used as a **format atlas**. Parsers in this repository are independently implemented. Do not vendor unlicensed code from other GbtG remake attempts.

## Ghidra / emulators

Load your own `default.xbe` in Ghidra. Use xemu or Cxbx-Reloaded as a behavior oracle against **your** disc. Do not commit Ghidra databases or Xbox images.
