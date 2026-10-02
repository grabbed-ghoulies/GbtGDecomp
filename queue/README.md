# Reconstruction queue

Local agents reconstruct Xbox functions one at a time. The supervisor in [`tools/decomp/supervisor.ps1`](../tools/decomp/supervisor.ps1) claims work, and a task is kept only after `gbtg` builds and `gbtg --headless --frames 1` exits 0.

Cloud agents cannot do this. `OriginalFiles/`, `ghidra_projects/`, and `cache/` are not in git.

## Owners

| Owner | Files |
|-------|--------|
| `frontend` | `src/engine/frontend.cpp` |
| `scene` | `src/engine/scene.cpp` |
| `entity` | `src/engine/entity.cpp`, `include/gbtg/entity.hpp` |
| `clock` | `src/engine/clock.cpp` |
| `audio` | `src/engine/audio.cpp` |
| `present` | `src/engine/gfx_d3d11.cpp`, `src/engine/gfx_null.cpp` |

`src/engine/game.cpp` is the thin `WinMain` / `frame_tick` caller. A task lists every file it may change. `exclusive: true` means it runs alone (shared headers such as `include/gbtg/game.hpp`).

## Task file

`queue/open/000FA3C0.md` is one function. The address is the filename. Folders are the state: `open`, `claimed`, `done`, `blocked`. Three failures, or a stale claim that has already been retried three times, lands in `blocked`.

`blocked_by` is a whitespace-separated list of `VA` keys. Those tasks must be in `queue/done/` first.

## Start

Commit this tooling first. Worktrees are created from `HEAD`, so uncommitted engine files are invisible to workers.

```text
.\tools\decomp\supervisor.cmd
```

That is `-Mode Loop`. It refills `queue/open` from Ghidra when fewer than three tasks are waiting, claims up to three tasks with different owners, and writes a worktree under `decomp-worktrees\` next to the repo. Each worktree gets a `TASK.md`.

If `agent` is on `PATH`, the supervisor runs `agent --print --force --workspace <worktree>` and then verifies. Otherwise it leaves `TASK.md` and a prompt at `queue/claimed/<VA>.prompt.md`. Run the workers, have each create `queue/claimed/<VA>.ready` when it finishes, then:

```text
.\tools\decomp\supervisor.ps1 -Mode Verify
```

`-Mode Status` prints the queue. `-Mode Once` does a single pass. `-NoRefill` skips Ghidra. Ctrl+C stops the loop. Nothing is pushed, and `main` is not updated. Successful tasks are cherry-picked onto the local branch `decomp/auto`.

Ghidra stays single-writer. Close Code Browser before a refill. The refill command is `ExportFrontier.java` in [docs/GHIDRA.md](../docs/GHIDRA.md).

Progress checks are listed in [MILESTONES.md](MILESTONES.md).
