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

## Headless export

Scripts in `tools/ghidra/ghidra_scripts/` run against the existing `ghidra_projects/GbtG` program `default.xbe.original` (import once in the GUI with **ghidra-xbe**). From this repo, using the in-tree Ghidra. Output paths follow the script file up to the repo root (override with `GBTG_ROOT`):

```text
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportEngine.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportWinMain.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportGameLoop.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportNext.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportScript.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportScriptLoad.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportCutscene.java
.\tools\ghidra\support\analyzeHeadless.bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportFrontier.java
```

`ExportEngine.java` writes `docs/generated/ghidra_functions.txt` (addresses and names only) plus `cache/ghidra_engine.txt` (decompiles). `ExportFrontier.java` decompiles the script-handler, stream, camera, and texture-stage seeds into `cache/ghidra_frontier/` and adds one task file per unnamed callee under `queue/open/` when that address is not already queued. `dump-only` skips the enqueue. Close Code Browser first, or the project may be locked.

When you rename a function in Ghidra, add a line to [`XBE.md`](XBE.md) and re-run `ExportEngine.java`.

## Oracle

Run the same disc in **xemu** (LLE) or Cxbx-Reloaded (HLE) when you want a side-effect check: which BNL is opened, which AID hash is requested, what D3D draw shows. The spec for a function is its decompile. The PC engine keeps that function's branches and offsets. A host boundary (file IO, D3D, XInput, heap, thread TLS) is only a call the dump makes, and it returns what the dump tests. `gbtg --headless` is a crash check. It does not accept the function.

## Headless on Linux

The scripts also run from a Linux shell against a copy of the project. Ghidra
needs a JDK 21 (with `javac`, for script compilation). The project's owner is
recorded as the Windows user, so pass that name to Java:

```text
export JAVA_HOME=<jdk-21>
export JAVA_TOOL_OPTIONS="-Duser.name=<project owner>"
<ghidra>/support/analyzeHeadless <copy of ghidra_projects> GbtG -process default.xbe.original -readOnly -noanalysis \
    -scriptPath tools/ghidra/ghidra_scripts -postScript CalleeTree.java <out dir> <translated list> 0019723C
```

`CalleeTree.java` walks callees breadth-first from the given roots (and from
`vt:<address>:<count>` vtable slots), stops at addresses in the translated
list, and writes `callee_tree.txt` plus a decompile for each untranslated
function. `DumpBytes.java <out dir> <start>-<end>` exports raw bytes for the
code oracle in `tools/oracle/`. Both are read-only.
