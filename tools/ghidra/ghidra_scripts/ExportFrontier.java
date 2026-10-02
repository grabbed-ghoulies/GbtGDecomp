// Queue unnamed callees of named functions. Read-only: does not rename or create functions.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportFrontier extends GbtgScript {
    private static final int kNewTaskCap = 12;
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
            refreshed = refreshOpenDumps(frontier);
            created = enqueueCallees(frontier, queued);
        } finally {
            decomp.dispose();
        }
        println("frontier refreshed=" + refreshed + " created=" + created);
    }

    private int refreshOpenDumps(File frontier) throws Exception {
        File open = queueDir("open");
        File[] files = open.listFiles();
        if (files == null) {
            return 0;
        }
        int n = 0;
        for (File md : files) {
            String key = keyOf(md);
            if (key == null) {
                continue;
            }
            long va = Long.parseLong(key, 16);
            Function fn = getFunctionAt(toAddr(va));
            writeDump(frontier, key, fn);
            n++;
        }
        return n;
    }

    private int enqueueCallees(File frontier, Set<String> queued) throws Exception {
        int created = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && created < kNewTaskCap) {
            Function caller = it.next();
            String callerName = caller.getName();
            if (callerName.startsWith("FUN_") || callerName.startsWith("thunk_FUN_")) {
                continue;
            }
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
                String name = callee.getName();
                if (!name.startsWith("FUN_") && !name.startsWith("thunk_FUN_")) {
                    continue;
                }
                long va = callee.getEntryPoint().getOffset();
                String key = String.format(Locale.ROOT, "%08X", va);
                if (queued.contains(key)) {
                    continue;
                }
                String owner = ownerFor(callerName, va);
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
        return "scene";
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
        if ("present".equals(owner)) {
            return "src/engine/gfx_d3d11.cpp; src/engine/gfx_null.cpp";
        }
        return "src/gbtg";
    }
}
