// Queue unnamed callees of named functions. Read-only: does not rename or create functions.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportFrontier extends GhidraScript {

    protected java.io.File repoRoot() {
        String env = System.getenv("GBTG_ROOT");
        if (env != null && env.trim().length() > 0) {
            return new java.io.File(env.trim());
        }
        java.io.File dir = sourceFile == null ? null : new java.io.File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new java.io.File(dir, "CMakeLists.txt").isFile() && new java.io.File(dir, "src").isDirectory()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        dir = new java.io.File(System.getProperty("user.dir"));
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new java.io.File(dir, "CMakeLists.txt").isFile() && new java.io.File(dir, "src").isDirectory()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        throw new IllegalStateException("GbtG repo root not found. Set GBTG_ROOT.");
    }

    protected java.io.File cacheFile(String name) {
        java.io.File cache = new java.io.File(repoRoot(), "cache");
        if (!cache.isDirectory() && !cache.mkdirs()) {
            throw new IllegalStateException("could not create " + cache.getAbsolutePath());
        }
        return new java.io.File(cache, name);
    }

    protected java.io.File repoFile(String relative) {
        java.io.File file = new java.io.File(repoRoot(), relative.replace('/', java.io.File.separatorChar));
        java.io.File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("could not create " + parent.getAbsolutePath());
        }
        return file;
    }

    private static final int kNewTaskCap = 12;
    // Skin palette and the mesh colour/texture-stage setup. The next tasks are
    // their callees, not every named function in the program.
    private static final long[] kSkinSeeds = {0x0002FAC0L, 0x0002F720L};
    private static final long[] kLightSeeds = {0x000644F0L, 0x0005E380L};
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        File frontier = new File(repoRoot(), "cache" + File.separator + "ghidra_frontier");
        if (!frontier.isDirectory() && !frontier.mkdirs()) {
            throw new IllegalStateException("could not create " + frontier.getAbsolutePath());
        }
        Set<String> queued = queuedKeys();
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        int refreshed = 0;
        int created = 0;
        try {
            refreshed = refreshQueuedDumps(frontier);
            created = enqueueCallees(frontier, queued);
        } finally {
            decomp.dispose();
        }
        println("frontier refreshed=" + refreshed + " created=" + created);
    }

    private int refreshQueuedDumps(File frontier) throws Exception {
        int n = 0;
        for (String folder : new String[] {"open", "claimed"}) {
            File dir = queueDir(folder);
            File[] files = dir.listFiles();
            if (files == null) {
                continue;
            }
            for (File md : files) {
                String key = keyOf(md);
                if (key == null) {
                    continue;
                }
                long va = Long.parseLong(key, 16);
                Function fn = getFunctionAt(toAddr(va));
                if (fn == null) {
                    fn = getFunctionContaining(toAddr(va));
                }
                writeDump(frontier, key, fn);
                n++;
            }
        }
        return n;
    }

    private void pushSeed(ArrayDeque<Function> walk, ArrayDeque<String> owners, Set<String> seen, long va,
            String owner) {
        Function fn = getFunctionAt(toAddr(va));
        if (fn == null) {
            fn = getFunctionContaining(toAddr(va));
        }
        if (fn == null) {
            return;
        }
        String key = String.format(Locale.ROOT, "%08X", fn.getEntryPoint().getOffset());
        if (!seen.add(key)) {
            return;
        }
        walk.addLast(fn);
        owners.addLast(owner);
    }

    private int enqueueCallees(File frontier, Set<String> queued) throws Exception {
        int created = 0;
        ArrayDeque<Function> walk = new ArrayDeque<Function>();
        ArrayDeque<String> owners = new ArrayDeque<String>();
        HashSet<String> seen = new HashSet<String>();
        for (long va : kSkinSeeds) {
            pushSeed(walk, owners, seen, va, "anim");
        }
        for (long va : kLightSeeds) {
            pushSeed(walk, owners, seen, va, "gfx");
        }
        while (!walk.isEmpty() && created < kNewTaskCap) {
            Function caller = walk.removeFirst();
            String owner = owners.removeFirst();
            Set<Function> called = caller.getCalledFunctions(monitor);
            if (called == null) {
                continue;
            }
            for (Function callee : called) {
                if (created >= kNewTaskCap) {
                    break;
                }
                if (callee == null || callee.isExternal() || callee.isThunk()) {
                    continue;
                }
                long va = callee.getEntryPoint().getOffset();
                String key = String.format(Locale.ROOT, "%08X", va);
                if (!seen.add(key)) {
                    continue;
                }
                String name = callee.getName();
                boolean unnamed = name.startsWith("FUN_") || name.startsWith("thunk_FUN_");
                if (!unnamed) {
                    walk.addLast(callee);
                    owners.addLast(owner);
                    continue;
                }
                if (queued.contains(key)) {
                    continue;
                }
                writeDump(frontier, key, callee);
                writeTask(key, va, "fn_" + Long.toHexString(va), owner, caller, callee);
                queued.add(key);
                created++;
            }
        }
        return created;
    }

    private void writeDump(File frontier, String key, Function fn) throws Exception {
        File out = new File(frontier, key + ".txt");
        PrintWriter w = new PrintWriter(new FileWriter(out), true);
        try {
            w.println("va=0x" + key);
            if (fn == null) {
                w.println("no function at this address");
                return;
            }
            w.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                    + " size=" + fn.getBody().getNumAddresses());
            DecompileResults res = decomp.decompileFunction(fn, 90, monitor);
            if (res != null && res.getDecompiledFunction() != null) {
                w.println(res.getDecompiledFunction().getC());
            } else {
                w.println("decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
            }
        } finally {
            w.close();
        }
    }

    private void writeTask(String key, long va, String name, String owner, Function caller, Function callee)
            throws Exception {
        File md = new File(queueDir("open"), key + ".md");
        PrintWriter w = new PrintWriter(new FileWriter(md), true);
        try {
            w.println("---");
            w.println("va: 0x" + key);
            w.println("name: " + name);
            w.println("owner: " + owner);
            w.println("status: open");
            w.println("retries: 0");
            w.println("exclusive: false");
            w.println("blocked_by:");
            w.println("decompile: cache/ghidra_frontier/" + key + ".txt");
            w.println("files: " + filesFor(owner));
            w.println("claimed:");
            w.println("error:");
            w.println("---");
            w.println();
            w.println("Unnamed callee of " + caller.getName() + " at 0x"
                    + String.format(Locale.ROOT, "%08X", caller.getEntryPoint().getOffset()) + ".");
            w.println();
            w.println("Callees:");
            w.println();
            int n = 0;
            Set<Function> subs = callee.getCalledFunctions(monitor);
            if (subs != null) {
                for (Function sub : subs) {
                    if (n++ >= 16) {
                        w.println("- ...");
                        break;
                    }
                    long subVa = sub.getEntryPoint().getOffset();
                    w.println("- 0x" + String.format(Locale.ROOT, "%08X", subVa) + " " + sub.getName());
                }
            }
            w.println();
            w.println("Reconstruct the behavior in the files listed above.");
            w.println("Leave a `// Xbox 0x" + key + "` comment on each function you add.");
            w.println("Do not edit any other file. Do not append docs/XBE.md. Do not commit.");
        } finally {
            w.close();
        }
    }

    private Set<String> queuedKeys() {
        TreeSet<String> keys = new TreeSet<String>();
        for (String dir : new String[] {"open", "claimed", "done", "blocked"}) {
            File folder = new File(repoRoot(), "queue" + File.separator + dir);
            File[] files = folder.listFiles();
            if (files == null) {
                continue;
            }
            for (File f : files) {
                String key = keyOf(f);
                if (key != null) {
                    keys.add(key);
                }
            }
        }
        return keys;
    }

    private static String keyOf(File file) {
        String name = file.getName();
        if (!name.endsWith(".md")) {
            return null;
        }
        String key = name.substring(0, name.length() - 3).toUpperCase(Locale.ROOT);
        if (key.length() != 8) {
            return null;
        }
        for (int i = 0; i < key.length(); i++) {
            char c = key.charAt(i);
            boolean hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
            if (!hex) {
                return null;
            }
        }
        return key;
    }

    private File queueDir(String name) {
        File dir = new File(repoRoot(), "queue" + File.separator + name);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IllegalStateException("could not create " + dir.getAbsolutePath());
        }
        return dir;
    }

    private static String ownerFor(String callerName, long va) {
        if (va == 0x15B780L || va == 0x3A040L || va == 0x396F0L || va == 0x38640L || va == 0x38C20L) {
            return "audio";
        }
        if (va == 0x33480L || va == 0x32AB0L || va == 0x33A90L) {
            return "present";
        }
        if (va == 0xFA3C0L || va == 0xFAD80L || va == 0xFC030L || va == 0xE9E60L || va == 0x33030L || va == 0x33C80L
                || va == 0x39D50L) {
            return "clock";
        }
        String n = callerName == null ? "" : callerName.toLowerCase(Locale.ROOT);
        if (n.contains("audio") || n.contains("xact") || n.contains("sound")) {
            return "audio";
        }
        if (n.contains("present") || n.contains("gfx")) {
            return "present";
        }
        if (n.contains("entity") || n.contains("actor") || n.contains("camera") || n.contains("banjo")
                || n.contains("cam_")) {
            return "entity";
        }
        if (n.contains("script") || n.contains("cutscene") || n.contains("scene") || n.contains("aidlist")) {
            return "scene";
        }
        if (n.contains("slot") || n.contains("keyboard") || n.contains("frontend") || n.contains("menu")
                || n.contains("kb")) {
            return "frontend";
        }
        if (n.contains("pad") || n.contains("clock") || n.contains("file") || n.contains("bundle")
                || n.contains("fs_") || n.contains("winmain") || n.contains("frame")) {
            return "clock";
        }
        return "anim";
    }

    private static String filesFor(String owner) {
        if ("frontend".equals(owner)) {
            return "src/engine/frontend.cpp";
        }
        if ("scene".equals(owner)) {
            return "src/engine/scene.cpp";
        }
        if ("entity".equals(owner)) {
            return "src/engine/entity.cpp; include/gbtg/entity.hpp";
        }
        if ("clock".equals(owner)) {
            return "src/engine/clock.cpp";
        }
        if ("audio".equals(owner)) {
            return "src/engine/audio.cpp";
        }
        if ("present".equals(owner) || "gfx".equals(owner)) {
            return "src/engine/gfx_d3d11.cpp";
        }
        if ("anim".equals(owner)) {
            return "src/gbtg/anim.cpp";
        }
        return "src/gbtg/anim.cpp";
    }
}
