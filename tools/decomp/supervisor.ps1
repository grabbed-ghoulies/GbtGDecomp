# Claims queue tasks, runs them in worktrees, and keeps a result only after
# gbtg builds and a headless frame exits 0. Commits land on local decomp/auto.
# Requires the engine split to be in HEAD (worktrees do not see uncommitted files).
param(
    [ValidateSet("Loop", "Once", "Status", "Verify")]
    [string]$Mode = "Once",
    [int]$MaxWorkers = 3,
    [int]$StaleMinutes = 45,
    [string]$GameDir = "",
    [switch]$NoRefill
)

# Continue: git writes fatal errors to stderr when a ref is missing. Those must not abort the pass.
$ErrorActionPreference = "Continue"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path (Join-Path $Repo "CMakeLists.txt"))) {
    throw "repo root not found from $PSScriptRoot"
}
if (-not $GameDir) {
    $GameDir = Join-Path $Repo "OriginalFiles"
}
$Queue = Join-Path $Repo "queue"
$WtRoot = Join-Path (Split-Path $Repo -Parent) "decomp-worktrees"
$Utf8 = New-Object System.Text.UTF8Encoding $false

function Read-Task([string]$Path) {
    $lines = [System.IO.File]::ReadAllLines($Path)
    $fields = @{}
    $inHeader = $false
    $headerDone = $false
    $body = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        if (-not $headerDone -and $line -eq "---") {
            if (-not $inHeader) {
                $inHeader = $true
                continue
            }
            $headerDone = $true
            $inHeader = $false
            continue
        }
        if ($inHeader) {
            $idx = $line.IndexOf(":")
            if ($idx -gt 0) {
                $fields[$line.Substring(0, $idx).Trim()] = $line.Substring($idx + 1).Trim()
            }
        }
        elseif ($headerDone) {
            $body.Add($line)
        }
    }
    [pscustomobject]@{
        Path   = $Path
        Fields = $fields
        Body   = ($body -join "`n").TrimEnd()
    }
}

function Write-Task($Task) {
    $keys = @(
        "va", "name", "owner", "status", "retries", "exclusive",
        "blocked_by", "decompile", "files", "claimed", "error"
    )
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("---")
    foreach ($key in $keys) {
        $value = $Task.Fields[$key]
        if ($null -eq $value) { $value = "" }
        [void]$sb.AppendLine("${key}: $value")
    }
    [void]$sb.AppendLine("---")
    [void]$sb.AppendLine("")
    if ($Task.Body) {
        [void]$sb.AppendLine($Task.Body)
    }
    [System.IO.File]::WriteAllText($Task.Path, $sb.ToString().TrimEnd() + "`n", $Utf8)
}

function Get-VaKey([string]$Va) {
    $hex = ($Va -replace "^0x", "").Trim()
    if (-not $hex) { return "" }
    return $hex.ToUpperInvariant().PadLeft(8, "0")
}

function Move-Task($Task, [string]$Folder) {
    $destDir = Join-Path $Queue $Folder
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    $dest = Join-Path $destDir (Split-Path $Task.Path -Leaf)
    if ((Resolve-Path $Task.Path).Path -ne (Join-Path $destDir (Split-Path $Task.Path -Leaf))) {
        Move-Item -LiteralPath $Task.Path -Destination $dest -Force
    }
    $Task.Path = $dest
    $Task.Fields["status"] = $Folder
    Write-Task $Task
}

function Get-TaskFiles([string]$Folder) {
    $dir = Join-Path $Queue $Folder
    if (-not (Test-Path $dir)) { return @() }
    return @(Get-ChildItem -LiteralPath $dir -Filter "*.md" | Sort-Object Name)
}

function Test-Unblocked($Task) {
    $raw = $Task.Fields["blocked_by"]
    if (-not $raw) { return $true }
    foreach ($part in ($raw -split "[\s,]+")) {
        $key = Get-VaKey $part
        if (-not $key) { continue }
        if (-not (Test-Path (Join-Path $Queue "done\$key.md"))) {
            return $false
        }
    }
    return $true
}

function Set-TaskError($Task, [string]$Message) {
    $flat = ($Message -replace "\s+", " ").Trim()
    if ($flat.Length -gt 500) { $flat = $flat.Substring(0, 500) }
    $Task.Fields["error"] = $flat
}

function Get-ClaimStamp($Task) {
    $key = Get-VaKey $Task.Fields["va"]
    $hb = Join-Path $Queue "claimed\$key.heartbeat"
    if (Test-Path $hb) {
        return (Get-Item -LiteralPath $hb).LastWriteTimeUtc
    }
    $claimed = $Task.Fields["claimed"]
    if ($claimed) {
        return [datetime]::Parse($claimed, $null, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
    }
    return (Get-Item -LiteralPath $Task.Path).LastWriteTimeUtc
}

function Remove-TaskWorktree([string]$Key) {
    if (-not $Key) { return }
    $path = Join-Path $WtRoot $Key
    & git -C $Repo worktree remove --force $path 2>$null
    & git -C $Repo branch -D "decomp/task-$Key" 2>$null
}

function Invoke-FailTask($Task, [string]$Message) {
    $retries = 0
    [void][int]::TryParse($Task.Fields["retries"], [ref]$retries)
    $retries++
    $Task.Fields["retries"] = "$retries"
    $Task.Fields["claimed"] = ""
    Set-TaskError $Task $Message
    $key = Get-VaKey $Task.Fields["va"]
    Remove-TaskWorktree $key
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.heartbeat") -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.ready") -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.prompt.md") -ErrorAction SilentlyContinue
    if ($retries -ge 3) {
        Move-Task $Task "blocked"
        Write-Host "blocked $key ($retries): $Message"
    }
    else {
        Move-Task $Task "open"
        Write-Host "retry $key ($retries): $Message"
    }
}

function Invoke-Reap {
    foreach ($file in (Get-TaskFiles "claimed")) {
        $task = Read-Task $file.FullName
        $key = Get-VaKey $task.Fields["va"]
        if (Test-Path (Join-Path $Queue "claimed\$key.ready")) { continue }
        $age = (Get-Date).ToUniversalTime() - (Get-ClaimStamp $task)
        if ($age.TotalMinutes -ge $StaleMinutes) {
            Invoke-FailTask $task "stale claim returned after $StaleMinutes minutes"
        }
    }
}

function Test-GhidraBusy {
    $project = Join-Path $Repo "ghidra_projects"
    if (-not (Test-Path $project)) { return $true }
    $locks = @(Get-ChildItem -LiteralPath $project -Recurse -Filter "*.lock" -ErrorAction SilentlyContinue)
    if ($locks.Count -gt 0) { return $true }
    $ours = Join-Path $Repo "cache\ghidra.lock"
    if (Test-Path $ours) {
        $age = (Get-Date) - (Get-Item -LiteralPath $ours).LastWriteTime
        if ($age.TotalMinutes -lt 180) { return $true }
    }
    return $false
}

function Invoke-Refill {
    if ($NoRefill) { return }
    $open = @(Get-TaskFiles "open")
    if ($open.Count -ge $MaxWorkers) { return }
    if (Test-GhidraBusy) {
        Write-Host "ghidra refill skipped (project missing, locked, or a refill is already running)"
        return
    }
    $bat = Join-Path $Repo "tools\ghidra\support\analyzeHeadless.bat"
    if (-not (Test-Path $bat)) {
        Write-Host "ghidra refill skipped (analyzeHeadless.bat missing)"
        return
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $Repo "cache") | Out-Null
    $lock = Join-Path $Repo "cache\ghidra.lock"
    Set-Content -LiteralPath $lock -Value $PID
    Write-Host "ghidra refill"
    Push-Location $Repo
    try {
        & $bat ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportFrontier.java
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ghidra refill exited $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
        Remove-Item -LiteralPath $lock -ErrorAction SilentlyContinue
    }
}

function Test-BaseCommit {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & git -C $Repo cat-file -e "HEAD:src/engine/frontend.cpp" 2>$null | Out-Null
    $ok = $LASTEXITCODE -eq 0
    $ErrorActionPreference = $prev
    return $ok
}

function Ensure-AutoWorktree {
    & git -C $Repo rev-parse --verify --quiet decomp/auto 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        & git -C $Repo branch decomp/auto HEAD
        if ($LASTEXITCODE -ne 0) { throw "could not create decomp/auto" }
    }
    New-Item -ItemType Directory -Force -Path $WtRoot | Out-Null
    $auto = Join-Path $WtRoot "auto"
    if (-not (Test-Path (Join-Path $auto ".git"))) {
        & git -C $Repo worktree add $auto decomp/auto
        if ($LASTEXITCODE -ne 0) { throw "could not add decomp/auto worktree" }
    }
    return $auto
}

function New-TaskWorktree($Task) {
    $key = Get-VaKey $Task.Fields["va"]
    $branch = "decomp/task-$key"
    $path = Join-Path $WtRoot $key
    & git -C $Repo rev-parse --verify --quiet $branch 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        & git -C $Repo worktree remove --force $path 2>$null
        & git -C $Repo branch -D $branch
        if ($LASTEXITCODE -ne 0) { throw "could not replace branch $branch" }
    }
    & git -C $Repo worktree add -b $branch $path decomp/auto
    if ($LASTEXITCODE -ne 0) { throw "could not add worktree $path" }
    return $path
}

function Write-WorkerPrompt($Task, [string]$Worktree) {
    $key = Get-VaKey $Task.Fields["va"]
    $va = $Task.Fields["va"]
    $name = $Task.Fields["name"]
    $owner = $Task.Fields["owner"]
    $files = $Task.Fields["files"]
    $dump = Join-Path $Repo ("cache\ghidra_frontier\" + $key + ".txt")
    $hb = Join-Path $Queue "claimed\$key.heartbeat"
    $ready = Join-Path $Queue "claimed\$key.ready"
    $text = @"
You are a reconstructor for Grabbed by the Ghoulies. Work only in this worktree.

VA: $va
Name: $name
Owner: $owner
Edit only: $files

Decompile dump (main checkout, not this worktree): $dump
Game directory for a manual run: $GameDir
Pass it as --game-dir. Do not stage OriginalFiles, xbe, bnl, or iso files.

Rules:
- Behavior reconstruction, not a paste of the Ghidra listing.
- Allman braces, matching src/engine.
- Comment each function you add with // Xbox $va
- Do not edit files that are not listed above.
- Do not append docs/XBE.md. Do not commit.
- Touch this heartbeat when you start and when you finish: $hb
- When you stop, create this empty file: $ready
"@
    $prompt = Join-Path $Queue "claimed\$key.prompt.md"
    [System.IO.File]::WriteAllText($prompt, $text.TrimEnd() + "`n", $Utf8)
    [System.IO.File]::WriteAllText((Join-Path $Worktree "TASK.md"), $text.TrimEnd() + "`n", $Utf8)
    Set-Content -LiteralPath $hb -Value (Get-Date).ToUniversalTime().ToString("o")
    return $prompt
}

function Invoke-Claim {
    if (-not (Test-BaseCommit)) {
        Write-Host "HEAD has no src/engine/frontend.cpp. Commit the engine split before claiming. Worktrees are created from commits."
        exit 1
    }
    $auto = Ensure-AutoWorktree
    $claimed = @()
    foreach ($file in (Get-TaskFiles "claimed")) {
        $claimed += Read-Task $file.FullName
    }
    $owners = @($claimed | ForEach-Object { $_.Fields["owner"] })
    $exclusive = @($claimed | Where-Object { $_.Fields["exclusive"] -eq "true" })
    if ($exclusive.Count -gt 0) { return }
    foreach ($file in (Get-TaskFiles "open")) {
        if ($claimed.Count -ge $MaxWorkers) { break }
        $task = Read-Task $file.FullName
        if ($task.Fields["exclusive"] -eq "true" -and $claimed.Count -gt 0) { continue }
        if ($owners -contains $task.Fields["owner"]) { continue }
        if (-not (Test-Unblocked $task)) { continue }
        Move-Task $task "claimed"
        $task.Fields["status"] = "claimed"
        $task.Fields["claimed"] = (Get-Date).ToUniversalTime().ToString("o")
        $task.Fields["error"] = ""
        Write-Task $task
        $wt = New-TaskWorktree $task
        $prompt = Write-WorkerPrompt $task $wt
        Write-Host "claimed $($task.Fields['va']) owner=$($task.Fields['owner']) worktree=$wt"
        Write-Host "prompt $prompt"
        $claimed += $task
        $owners += $task.Fields["owner"]
        if ($task.Fields["exclusive"] -eq "true") { break }
    }
    return $auto
}

function Get-DirtyPaths([string]$Worktree) {
    $rows = @(& git -C $Worktree status --porcelain)
    $paths = @()
    foreach ($row in $rows) {
        if ($row.Length -lt 4) { continue }
        $path = $row.Substring(3).Trim()
        if ($path -match " -> ") {
            $path = ($path -split " -> ")[-1].Trim()
        }
        $paths += ($path -replace "\\", "/")
    }
    return $paths
}

function Get-ChangedPaths([string]$Worktree) {
    $names = @(& git -C $Worktree diff --name-only "decomp/auto...HEAD")
    $names += @(Get-DirtyPaths $Worktree)
    return @($names | Where-Object { $_ -and $_ -ne "TASK.md" } | ForEach-Object { $_ -replace "\\", "/" } | Select-Object -Unique)
}

function Add-XbeLine([string]$Worktree, $Task) {
    $path = Join-Path $Worktree "docs\XBE.md"
    $va = $Task.Fields["va"]
    $name = $Task.Fields["name"]
    $owner = $Task.Fields["owner"]
    $text = [System.IO.File]::ReadAllText($path)
    $marker = "## Supervisor log"
    if ($text -notmatch [regex]::Escape($marker)) {
        $text = $text.TrimEnd() + "`n`n## Supervisor log`n`n| VA | Name | Notes |`n|----|------|-------|`n"
    }
    $row = "| ``$va`` | ``$name`` | $owner |"
    $tail = $text.Substring($text.IndexOf($marker))
    if ($tail -notlike "*$va*") {
        $text = $text.TrimEnd() + "`n" + $row + "`n"
        [System.IO.File]::WriteAllText($path, $text, $Utf8)
    }
}

function Invoke-VerifyTask($Task) {
    $key = Get-VaKey $Task.Fields["va"]
    $wt = Join-Path $WtRoot $key
    $auto = Join-Path $WtRoot "auto"
    if (-not (Test-Path $wt)) {
        Invoke-FailTask $Task "worktree missing at $wt"
        return
    }
    $allowed = @()
    foreach ($file in ($Task.Fields["files"] -split ";")) {
        $rel = $file.Trim() -replace "\\", "/"
        if ($rel) { $allowed += $rel }
    }
    $changed = @(Get-ChangedPaths $wt)
    if ($changed.Count -eq 0) {
        Invoke-FailTask $Task "no edits"
        return
    }
    foreach ($path in $changed) {
        if ($allowed -notcontains $path) {
            Invoke-FailTask $Task "edited $path which is outside the task"
            return
        }
    }
    $build = Join-Path $wt "build"
    Write-Host "verify $key"
    & cmake -S $wt -B $build
    if ($LASTEXITCODE -ne 0) {
        Invoke-FailTask $Task "cmake configure failed"
        return
    }
    & cmake --build $build --config Release --target gbtg
    if ($LASTEXITCODE -ne 0) {
        Invoke-FailTask $Task "build failed"
        return
    }
    $exe = Join-Path $build "Release\gbtg.exe"
    & $exe --game-dir $GameDir --headless --frames 1
    if ($LASTEXITCODE -ne 0) {
        Invoke-FailTask $Task "headless exit $LASTEXITCODE"
        return
    }
    $va = $Task.Fields["va"]
    $name = $Task.Fields["name"]
    $dirtySource = @((Get-DirtyPaths $wt) | Where-Object { $_ -ne "TASK.md" })
    if ($dirtySource.Count -gt 0) {
        & git -C $wt add -- $allowed
        if ($LASTEXITCODE -ne 0) {
            Invoke-FailTask $Task "git add failed"
            return
        }
        & git -C $wt commit -m "reconstruct $name at $va"
        if ($LASTEXITCODE -ne 0) {
            & git -C $wt reset
            Invoke-FailTask $Task "git commit failed"
            return
        }
    }
    & git -C $auto cherry-pick "decomp/auto..decomp/task-$key"
    if ($LASTEXITCODE -ne 0) {
        & git -C $auto cherry-pick --abort 2>$null
        Invoke-FailTask $Task "cherry-pick onto decomp/auto failed"
        return
    }
    Add-XbeLine $auto $Task
    & git -C $auto add -- docs/XBE.md
    & git -C $auto commit --amend --no-edit
    if ($LASTEXITCODE -ne 0) {
        & git -C $auto commit -m "note $name at $va"
    }
    & git -C $Repo worktree remove --force $wt
    & git -C $Repo branch -d "decomp/task-$key"
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.heartbeat") -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.ready") -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $Queue "claimed\$key.prompt.md") -ErrorAction SilentlyContinue
    $Task.Fields["error"] = ""
    $Task.Fields["claimed"] = ""
    Move-Task $Task "done"
    Write-Host "done $va on decomp/auto"
}

function Invoke-VerifyReady {
    foreach ($file in (Get-TaskFiles "claimed")) {
        $task = Read-Task $file.FullName
        $key = Get-VaKey $task.Fields["va"]
        if (Test-Path (Join-Path $Queue "claimed\$key.ready")) {
            Invoke-VerifyTask $task
        }
    }
}

function Invoke-Agents {
    $agent = Get-Command agent -ErrorAction SilentlyContinue
    $jobs = @()
    foreach ($file in (Get-TaskFiles "claimed")) {
        $task = Read-Task $file.FullName
        $key = Get-VaKey $task.Fields["va"]
        if (Test-Path (Join-Path $Queue "claimed\$key.ready")) { continue }
        $wt = Join-Path $WtRoot $key
        $promptPath = Join-Path $wt "TASK.md"
        if (-not (Test-Path $promptPath)) { continue }
        if (-not $agent) {
            Write-Host "no agent CLI. Worker prompt: $promptPath"
            continue
        }
        Write-Host "agent $key"
        $arg = "--print --force --workspace `"$wt`" `"Follow TASK.md in this workspace. Create the ready file when you stop.`""
        $jobs += Start-Process -FilePath $agent.Source -ArgumentList $arg -PassThru -NoNewWindow
        $jobs[-1] | Add-Member -NotePropertyName TaskKey -NotePropertyValue $key
    }
    if (-not $agent) { return $false }
    $launched = @()
    foreach ($job in $jobs) {
        $job.WaitForExit()
        Write-Host "agent $($job.TaskKey) exited $($job.ExitCode)"
        $launched += $job.TaskKey
    }
    foreach ($file in (Get-TaskFiles "claimed")) {
        $task = Read-Task $file.FullName
        $key = Get-VaKey $task.Fields["va"]
        if ($launched -contains $key) {
            Invoke-VerifyTask $task
        }
    }
    return $true
}

function Show-Status {
    foreach ($folder in @("open", "claimed", "done", "blocked")) {
        $files = @(Get-TaskFiles $folder)
        Write-Host "$folder $($files.Count)"
        foreach ($file in $files) {
            $task = Read-Task $file.FullName
            $err = $task.Fields["error"]
            $suffix = ""
            if ($err) { $suffix = " error=$err" }
            Write-Host ("  {0} {1} {2}{3}" -f $task.Fields["va"], $task.Fields["owner"], $task.Fields["name"], $suffix)
        }
    }
}

function Invoke-Pass {
    Invoke-Reap
    Invoke-VerifyReady
    Invoke-Refill
    if ($Mode -eq "Verify") { return }
    Invoke-Claim | Out-Null
    Invoke-Agents | Out-Null
}

switch ($Mode) {
    "Status" { Show-Status }
    "Verify" { Invoke-VerifyReady; Show-Status }
    "Once" { Invoke-Pass; Show-Status }
    "Loop" {
        while ($true) {
            Invoke-Pass
            Show-Status
            $agent = Get-Command agent -ErrorAction SilentlyContinue
            if (-not $agent) {
                Write-Host "Loop stopped: agent is not on PATH. Re-run -Mode Verify after workers write queue/claimed/<VA>.ready"
                break
            }
            Start-Sleep -Seconds 20
        }
    }
}
