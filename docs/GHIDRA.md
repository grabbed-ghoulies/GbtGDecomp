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

`ExportEngine.java` writes `docs/generated/ghidra_functions.txt` (addresses and names only) plus `cache/ghidra_engine.txt` (decompiles). `ExportFrontier.java` decompiles unnamed callees of named functions into `cache/ghidra_frontier/` and adds one task file per function under `queue/open/` when that address is not already queued. Close Code Browser first, or the project may be locked.

When you rename a function in Ghidra, add a line to [`XBE.md`](XBE.md) and re-run `ExportEngine.java`.

## Oracle

Run the same disc in **xemu** (LLE) or Cxbx-Reloaded (HLE). When reconstructing a function (file open, bundle load, actor spawn), compare side effects: which BNL is opened, which AID hash is requested, what D3D draw shows. The PC engine should match behavior, not bytes.
