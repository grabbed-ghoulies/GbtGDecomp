# Legal

GbtGDecomp is original software written from reverse-engineering a legally obtained copy of *Grabbed by the Ghoulies*.

## What this project ships

- Source code, build files, and documentation authored by this project
- File **checksums and sizes** used to verify a user-supplied dump
- Format notes derived from inspecting the Xbox executable and asset containers

## What this project does not ship

- Any file from the Xbox disc (`default.xbe`, `.bnl`, hashed assets, audio, Bink movies, promo trailers)
- Microsoft Xbox Development Kit headers or libraries
- RAD Game Tools Bink SDK or decoder
- Copies of third-party reverse-engineering trees that have no license

End users must supply their own retail files. Tools accept `--game-dir` (an extracted disc folder). ISO extraction is a later convenience, not a distribution channel.

## Bink movies

In-game cutscenes in `movie/` are Bink (`BIKi`). Do not add a Bink runtime to this tree. Convert locally if you need PC playback (for example with a tool you already own), or skip movies until a local transcoder is wired into the extractor.

## Third-party research

Public write-ups of Rare AID hashing, CAFF/BNL layout, and Xbox texture tiling were used as a **format atlas**. Parsers in this repository are independently implemented. Do not vendor unlicensed code from other GbtG remake attempts.

## Ghidra / emulators

Load your own `default.xbe` in Ghidra. Use xemu or Cxbx-Reloaded as a behavior oracle against **your** disc. Do not commit Ghidra databases or Xbox images.
