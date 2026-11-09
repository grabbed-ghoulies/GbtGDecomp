-- GbtG tools for 3ds Max 2021 or older (docs/INSPECTOR.md, "3ds Max tools").
--
-- A "GbtG Material" (a DirectX Shader on gbtg_cel.fx with the game's cel ramp and lighting blob) and a GbtG Tools panel:
--   Export from game   runs gbtg-inspector --export-model, which writes <aid>.fbx, <aid>.fbm\ (every texture as PNG)
--                      and <aid>.gbtg.ini (the model's materials, their part and textures, and the inspector's path).
--   Load exported FBX  imports it with skin and morphs and turns its materials into GbtG Materials.
--   New GbtG Material  a new material on one of the model's materials (its base) with its own colour map.
--   Send to game       exports binary FBX with game names for the materials and runs gbtg-inspector --import-model
--                      into the plugin folder. An open inspector reloads the scene when the file changes.
--
-- Install: run this file (Scripting > Run Script), or put a one-line startup script in
-- %LOCALAPPDATA%\Autodesk\3dsMax\2021 - 64bit\ENU\scripts\startup that does fileIn on it, so scenes saved with GbtG
-- Materials open with the material defined. Customize User Interface > Category "GbtG" has the "GbtG Tools" action.
-- Keep gbtg_cel.fx beside this file.

global GbtG_ScriptDir = getFilenamePath (getThisScriptFilename())
global GbtG_EffectFile = GbtG_ScriptDir + "gbtg_cel.fx"
global GbtG_Tools
global GbtG_ToolsRollout

-- The materials an export manifest lists: one entry per name, #(name, part, colour, celmap, lightmap) with full paths.
fn GbtG_ReadManifest manifest =
(
    local result = #()
    if manifest == undefined or manifest == "" or not doesFileExist manifest then return result
    local folder = getFilenamePath manifest + (getINISetting manifest "Model" "textures") + "\\"
    local count = (getINISetting manifest "Materials" "count") as integer
    if count == undefined then count = 0
    fn full folder name = (if name == "" then "" else folder + name)
    for i = 1 to count do
    (
        local section = "Material" + i as string
        local name = getINISetting manifest section "name"
        local known = false
        for entry in result where entry[1] == name do known = true
        if not known and name != "" then
        (
            append result #(name, getINISetting manifest section "part", \
                full folder (getINISetting manifest section "colour"), \
                full folder (getINISetting manifest section "celmap"), \
                full folder (getINISetting manifest section "lightmap"))
        )
    )
    result
)

fn GbtG_ManifestEntry manifest name =
(
    local found = undefined
    for entry in (GbtG_ReadManifest manifest) where entry[1] == name do found = entry
    found
)

plugin material GbtGMaterial
    name:"GbtG Material"
    classID:#(0x47627447, 0x4d617431)
    extends:DirectX_9_Shader
    replaceUI:false
    version:1
(
    parameters main rollout:gbtgParams
    (
        manifest type:#string default:""
        baseName type:#string default:""
        part type:#string default:"body"
        isNew type:#boolean default:false
        newName type:#string default:""
        colourFile type:#string default:""
        celFile type:#string default:""
        lightFile type:#string default:""
    )

    -- The name the importer reads: the base for one of the model's materials, else <name>__<base>.
    fn exportName =
    (
        if isNew and newName != "" then newName + "__" + baseName else baseName
    )

    fn loadMap file =
    (
        if file != "" and doesFileExist file then openBitmap file else undefined
    )

    -- Points the cel shader at the files and names the material for export.
    fn refresh =
    (
        if delegate.effectFile != GbtG_EffectFile then delegate.effectFile = GbtG_EffectFile
        local colour = loadMap colourFile
        delegate.g_UseColourTexture = colour != undefined
        if colour != undefined then delegate.g_ColourTexture = colour
        local ramp = loadMap celFile
        delegate.g_UseCelTexture = ramp != undefined
        if ramp != undefined then delegate.g_CelTexture = ramp
        local blob = loadMap lightFile
        delegate.g_UseLightTexture = blob != undefined
        if blob != undefined then delegate.g_LightTexture = blob
        if baseName != "" then this.name = exportName()
    )

    -- Takes the base's part, ramp and blob (and its colour map unless this material has its own).
    fn setBase name keepColour:false =
    (
        local entry = GbtG_ManifestEntry manifest name
        baseName = name
        if entry != undefined then
        (
            part = entry[2]
            if not keepColour or colourFile == "" then colourFile = entry[3]
            celFile = entry[4]
            lightFile = entry[5]
        )
        refresh()
    )

    rollout gbtgParams "GbtG Material"
    (
        label lblExport "" align:#left
        dropdownlist ddBase "Base (the model's material):" items:#()
        checkbox chkNew "New material on this base"
        edittext edName "Name:" enabled:false
        button btnColour "Colour map..." width:140 across:2
        button btnClear "Base's colour" width:100
        label lblColour "" align:#left
        label lblPart "" align:#left

        fn update =
        (
            local entries = GbtG_ReadManifest manifest
            ddBase.items = for e in entries collect e[1]
            ddBase.selection = 0
            for i = 1 to entries.count where entries[i][1] == baseName do ddBase.selection = i
            chkNew.checked = isNew
            edName.text = newName
            edName.enabled = isNew
            lblExport.text = "Exports as: " + (if baseName == "" then "(choose a base)" else exportName())
            lblColour.text = if colourFile == "" then "Colour: none (ramp only)" else "Colour: " + filenameFromPath colourFile
            lblPart.text = "Part: " + part + (if manifest == "" then "  (no manifest: load an exported FBX)" else "")
        )

        on gbtgParams open do update()
        on ddBase selected i do
        (
            setBase ddBase.items[i] keepColour:isNew
            update()
        )
        on chkNew changed state do
        (
            isNew = state
            refresh()
            update()
        )
        on edName entered text do
        (
            newName = substituteString (trimLeft (trimRight text)) " " "_"
            refresh()
            update()
        )
        on btnColour pressed do
        (
            local file = getOpenFileName caption:"Colour map" types:"Images (*.png;*.tga;*.bmp;*.jpg)|*.png;*.tga;*.bmp;*.jpg|All|*.*"
            if file != undefined then
            (
                colourFile = file
                refresh()
                update()
            )
        )
        on btnClear pressed do
        (
            local entry = GbtG_ManifestEntry manifest baseName
            if entry != undefined then colourFile = entry[3]
            refresh()
            update()
        )
    )

    on create do
    (
        delegate.effectFile = GbtG_EffectFile
    )
)

struct GbtG_ToolsStruct
(
    inspector = "",
    gameDir = "",
    pluginDir = "",
    aid = "aid_model_ghoulies_actor_boy",
    workDir = "",
    manifest = "",
    lastLog = "",

    fn settingsFile = (getDir #plugcfg) + "\\gbtg_tools.ini",

    fn load =
    (
        local file = settingsFile()
        fn read file key fallback = (local v = getINISetting file "GbtG" key; if v == "" then fallback else v)
        inspector = read file "inspector" inspector
        gameDir = read file "gameDir" gameDir
        pluginDir = read file "pluginDir" pluginDir
        aid = read file "aid" aid
        workDir = read file "workDir" ((getDir #export) + "\\gbtg")
        manifest = read file "manifest" manifest
    ),

    fn save =
    (
        local file = settingsFile()
        setINISetting file "GbtG" "inspector" inspector
        setINISetting file "GbtG" "gameDir" gameDir
        setINISetting file "GbtG" "pluginDir" pluginDir
        setINISetting file "GbtG" "aid" aid
        setINISetting file "GbtG" "workDir" workDir
        setINISetting file "GbtG" "manifest" manifest
    ),

    -- The inspector's plugins\my_models unless a folder is set; the inspector watches its plugins folder.
    fn pluginFolder =
    (
        if pluginDir != "" then pluginDir else (getFilenamePath inspector) + "plugins\\my_models"
    ),

    -- Runs the inspector with `arguments` through a batch file; returns #(exit code, output).
    fn runInspector arguments =
    (
        if inspector == "" or not doesFileExist inspector then return #(-1, "Set the inspector (gbtg-inspector.exe) first.")
        makeDir workDir all:true
        local bat = workDir + "\\gbtg_run.bat"
        local out = workDir + "\\gbtg_run.log"
        local code = workDir + "\\gbtg_run.code"
        deleteFile out
        deleteFile code
        local f = createFile bat
        format "@echo off\r\n\"%\" % > \"%\" 2>&1\r\necho \%ERRORLEVEL\% > \"%\"\r\n" inspector arguments out code to:f
        close f
        HiddenDOSCommand ("\"" + bat + "\"") startpath:(getFilenamePath inspector) donotwait:false
        local text = ""
        if doesFileExist out then
        (
            local s = openFile out
            while not eof s do text += readLine s + "\n"
            close s
        )
        local exitCode = -1
        if doesFileExist code then
        (
            local s = openFile code
            exitCode = (trimRight (readLine s)) as integer
            close s
        )
        #(exitCode, text)
    ),

    fn quoted s = "\"" + s + "\"",

    -- Turns the scene's materials named in the manifest into GbtG Materials (inside multi/sub-objects too).
    fn convertMaterials =
    (
        local converted = 0
        local made = #()
        local names = #()
        fn convertOne m manifest names made =
        (
            if m == undefined or classOf m == GbtGMaterial then return m
            local entry = GbtG_ManifestEntry manifest m.name
            if entry == undefined then return m
            local i = findItem names m.name
            if i > 0 then return made[i]
            local g = GbtGMaterial()
            g.manifest = manifest
            g.setBase entry[1]
            append names m.name
            append made g
            g
        )
        for o in geometry where o.material != undefined do
        (
            local mat = o.material
            if classOf mat == Multimaterial then
            (
                for i = 1 to mat.materialList.count do
                (
                    local before = mat.materialList[i]
                    local after = convertOne before manifest names made
                    if after != before then (mat.materialList[i] = after; converted += 1)
                )
            )
            else
            (
                local after = convertOne mat manifest names made
                if after != mat then (o.material = after; converted += 1)
            )
        )
        converted
    ),

    -- Imports an exported FBX (skin and morphs) and reads its manifest.
    fn loadExported fbx =
    (
        local ini = getFilenamePath fbx + getFilenameFile fbx + ".gbtg.ini"
        if not doesFileExist ini then return ("No manifest beside it: " + ini + ". Export the model from the inspector first.")
        manifest = ini
        local manifestAid = getINISetting ini "Model" "aid"
        if manifestAid != "" then aid = manifestAid
        local manifestInspector = getINISetting ini "Model" "inspector"
        if inspector == "" and manifestInspector != "" then inspector = manifestInspector
        local manifestGame = getINISetting ini "Model" "game_dir"
        if gameDir == "" and manifestGame != "" then gameDir = manifestGame
        workDir = trimRight (getFilenamePath fbx) "\\"
        FBXImporterSetParam "Mode" #create
        FBXImporterSetParam "Skin" true
        FBXImporterSetParam "Shape" true
        FBXImporterSetParam "ShowWarnings" false
        importFile fbx #noPrompt using:FBXIMP
        local count = convertMaterials()
        save()
        "Loaded " + filenameFromPath fbx + "; " + count as string + " material slots are GbtG Materials."
    ),

    -- Exports the model from the game's disc into the work folder, then loads it.
    fn exportFromGame =
    (
        local fbx = workDir + "\\" + aid + ".fbx"
        local run = runInspector ("--export-model " + aid + " --game-dir " + (quoted gameDir) + " --out " + (quoted fbx))
        lastLog = run[2]
        if run[1] != 0 then return ("Export failed (" + run[1] as string + "):\n" + run[2])
        run[2] + "\n" + (loadExported fbx)
    ),

    -- Exports the scene as binary FBX with the GbtG Materials as plain materials named for the importer, then imports
    -- it into the plugin folder. The scene's materials are put back afterwards.
    fn sendToGame =
    (
        if aid == "" then return "Set the model AID first."
        makeDir workDir all:true
        local originals = #()
        local plain = #()
        fn plainFor g originals plain =
        (
            local i = findItem originals g
            if i > 0 then return plain[i]
            local s = Standardmaterial name:(g.exportName())
            if g.colourFile != "" and doesFileExist g.colourFile then
            (
                s.diffuseMap = Bitmaptexture filename:g.colourFile name:(getFilenameFile g.colourFile)
                s.showInViewport = true
            )
            append originals g
            append plain s
            s
        )
        local swaps = #() -- #(owner, slot or 0, original)
        for o in geometry where o.material != undefined do
        (
            local mat = o.material
            if classOf mat == GbtGMaterial then
            (
                append swaps #(o, 0, mat)
                o.material = plainFor mat originals plain
            )
            else if classOf mat == Multimaterial then
            (
                for i = 1 to mat.materialList.count where classOf mat.materialList[i] == GbtGMaterial do
                (
                    append swaps #(mat, i, mat.materialList[i])
                    mat.materialList[i] = plainFor mat.materialList[i] originals plain
                )
            )
        )
        local fbx = workDir + "\\" + aid + "_send.fbx"
        local exported = false
        try
        (
            FBXExporterSetParam "ASCII" false
            FBXExporterSetParam "Skin" true
            FBXExporterSetParam "Shape" true
            FBXExporterSetParam "Animation" false
            FBXExporterSetParam "EmbedTextures" true
            FBXExporterSetParam "ShowWarnings" false
            exported = exportFile fbx #noPrompt using:FBXEXP
        )
        catch (lastLog = getCurrentException())
        for s in swaps do
        (
            if s[2] == 0 then s[1].material = s[3] else s[1].materialList[s[2]] = s[3]
        )
        if not exported then return ("FBX export failed. " + lastLog)
        local run = runInspector ("--import-model " + (quoted fbx) + " --as " + aid + " --game-dir " + (quoted gameDir) \
            + " --plugin " + (quoted (pluginFolder())))
        lastLog = run[2]
        save()
        (if run[1] == 0 then "Sent " + aid + " to " + pluginFolder() + " (" + originals.count as string + " GbtG Materials).\n" \
            else "Import failed (" + run[1] as string + "):\n") + run[2]
    ),

    -- A new GbtG Material on the first of the model's materials, in the active Material Editor slot and on the
    -- selection's faces when one mesh is selected with no material.
    fn newMaterial =
    (
        local g = GbtGMaterial()
        g.manifest = manifest
        g.isNew = true
        g.newName = "new_material"
        local entries = GbtG_ReadManifest manifest
        if entries.count > 0 then g.setBase entries[1][1]
        meditMaterials[activeMeditSlot] = g
        g
    ),

    fn open =
    (
        load()
        try (destroyDialog GbtG_ToolsRollout) catch ()
        createDialog GbtG_ToolsRollout width:360
    )
)

GbtG_Tools = GbtG_ToolsStruct()
GbtG_Tools.load()

rollout GbtG_ToolsRollout "GbtG Tools"
(
    group "Setup"
    (
        edittext edInspector "Inspector:" labelOnTop:true across:1
        button btnInspector "Browse inspector..." align:#right
        edittext edGame "Game folder (disc files):" labelOnTop:true
        edittext edPlugin "Plugin folder (empty: inspector's plugins\\my_models):" labelOnTop:true
        edittext edAid "Model AID:" labelOnTop:true
        edittext edWork "Work folder:" labelOnTop:true
    )
    group "Model"
    (
        button btnExport "Export from game and load" width:320
        button btnLoad "Load exported FBX..." width:320
        button btnNew "New GbtG Material (Material Editor)" width:320
        button btnSend "Send to game" width:320 height:32
    )
    edittext edLog "" height:160 readOnly:true

    fn pull =
    (
        GbtG_Tools.inspector = edInspector.text
        GbtG_Tools.gameDir = edGame.text
        GbtG_Tools.pluginDir = edPlugin.text
        GbtG_Tools.aid = edAid.text
        GbtG_Tools.workDir = edWork.text
        GbtG_Tools.save()
    )
    fn push =
    (
        edInspector.text = GbtG_Tools.inspector
        edGame.text = GbtG_Tools.gameDir
        edPlugin.text = GbtG_Tools.pluginDir
        edAid.text = GbtG_Tools.aid
        edWork.text = GbtG_Tools.workDir
    )
    fn show text = (edLog.text = substituteString text "\n" "\r\n")

    on GbtG_ToolsRollout open do push()
    on btnInspector pressed do
    (
        local file = getOpenFileName caption:"gbtg-inspector.exe" types:"Inspector|gbtg-inspector*.exe|All|*.*"
        if file != undefined then (edInspector.text = file; pull())
    )
    on btnExport pressed do
    (
        pull()
        show "Exporting..."
        show (GbtG_Tools.exportFromGame())
        push()
    )
    on btnLoad pressed do
    (
        pull()
        local file = getOpenFileName caption:"Exported model" filename:(GbtG_Tools.workDir + "\\") types:"FBX|*.fbx"
        if file != undefined then (show (GbtG_Tools.loadExported file); push())
    )
    on btnNew pressed do
    (
        pull()
        local g = GbtG_Tools.newMaterial()
        MatEditor.Open()
        show ("Made " + g.name + " in Material Editor slot " + activeMeditSlot as string \
            + ". Name it, pick its base and colour map, and apply it to faces.")
    )
    on btnSend pressed do
    (
        pull()
        show "Sending..."
        show (GbtG_Tools.sendToGame())
    )
)

macroScript GbtGToolsPanel category:"GbtG" buttonText:"GbtG Tools" tooltip:"GbtG model tools"
(
    fileIn (GbtG_ScriptDir + "gbtg_tools.ms")
    GbtG_Tools.open()
)
