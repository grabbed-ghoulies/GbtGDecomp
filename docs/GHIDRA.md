# Ghidra workflow

Analyze **your** `default.xbe`. Nothing from the disc belongs in git.

## Install

1. [Ghidra](https://ghidra-sre.org/) 11.x
2. [XboxDev/ghidra-xbe](https://github.com/XboxDev/ghidra-xbe) loader + XbSymbolDatabase

Install the extension: **File → Install Extensions…** → `XboxExecutableLoader`.

## Import (do not load as a flat binary)

XBE sections are not contiguous in the file. A flat load puts every pointer on the wrong byte.

1. **File → New Project** (keep it outside the git tree, e.g. `ghidra_projects/` which is gitignored).
2. **Import** `default.xbe` with the **Xbox executable** loader (language `x86:LE:32:default`).
3. Analyze with default analyzers plus **Xbox** symbol recovery if offered.
4. Optional: parse public nxdk/`xbox.h` types (**File → Parse C Source**) — never commit XDK headers.

## Headless helper

`tools/ghidra/ExportGbtg.java` prints AID-like strings and xrefs. From a Ghidra install:

```text
analyzeHeadless <projectDir> GbtG -import "<game-dir>/default.xbe" -processor x86:LE:32:default -postScript ExportGbtg.java
```

Use ghidra-xbe so the import is not a raw binary. If headless cannot load XBE, import once in the GUI, then run scripts on the existing program.

## Oracle

Run the same disc in **xemu** (LLE) or Cxbx-Reloaded (HLE). When reconstructing a function (file open, bundle load, actor spawn), compare side effects: which BNL is opened, which AID hash is requested, what D3D draw shows. The PC engine should match behavior, not bytes.
