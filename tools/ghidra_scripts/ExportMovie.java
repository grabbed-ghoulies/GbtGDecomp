// Read-only dump of the game movie player. Stops at the BINK section.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportMovie extends GhidraScript {

    private static final long kBink = 0x1A59E0L;
    private PrintWriter out;
    private DecompInterface decomp;
    private final Set<Long> dumped = new LinkedHashSet<Long>();

    private File repoRoot() {
        String env = System.getenv("GBTG_ROOT");
        if (env != null && env.trim().length() > 0) {
            return new File(env.trim());
        }
        File dir = sourceFile == null ? null : new File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new File(dir, "CMakeLists.txt").isFile() && new File(dir, "src").isDirectory()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        dir = new File(System.getProperty("user.dir"));
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new File(dir, "CMakeLists.txt").isFile() && new File(dir, "src").isDirectory()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        throw new IllegalStateException("GbtG repo root not found. Set GBTG_ROOT.");
    }

    @Override
    public void run() throws Exception {
        File cache = new File(repoRoot(), "cache");
        if (!cache.isDirectory() && !cache.mkdirs()) {
            throw new IllegalStateException("could not create " + cache.getAbsolutePath());
        }
        out = new PrintWriter(new FileWriter(new File(cache, "ghidra_movie.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        if (!decomp.openProgram(currentProgram)) {
            out.println("decompiler: " + decomp.getLastMessage());
            out.close();
            return;
        }
        try {
            dumpOne(0x107420L);
            dumpXrefs(0x50E0B8L, "DAT_0050e0b8");
            dumpXrefs(0x1A6010L, "bink_1a6010");
            dumpXrefs(0x4E5CD8L, "movie_lock_a");
            dumpXrefs(0x4E5CDCL, "movie_lock_b");
            dumpString("movie\\");
            dumpString("d:\\movie\\0de65e94");
            dumpString("d:\\movie\\");
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_movie.txt");
    }

    private void dumpString(String needle) {
        out.println("== string " + needle + " ==");
        Memory mem = currentProgram.getMemory();
        byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
        Address from = currentProgram.getMinAddress();
        int hits = 0;
        while (from != null && hits < 16) {
            Address hit = mem.findBytes(from, raw, null, true, monitor);
            if (hit == null) {
                break;
            }
            out.println("  str " + hit);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(hit);
            int rc = 0;
            while (refs.hasNext() && rc < 24) {
                Reference r = refs.next();
                Function fn = getFunctionContaining(r.getFromAddress());
                out.println("    xref " + r.getFromAddress() + (fn == null ? "" : "  " + fn.getName() + " " + fn.getEntryPoint()));
                if (fn != null) {
                    dumpFunction(fn);
                }
                rc++;
            }
            from = hit.add(1);
            hits++;
        }
        out.println();
    }

    private void dumpXrefs(long va, String tag) {
        out.println("== xrefs to " + tag + " @ " + Long.toHexString(va) + " ==");
        Address addr = toAddr(va);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        while (refs.hasNext()) {
            Reference r = refs.next();
            long from = r.getFromAddress().getOffset();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + " type=" + r.getReferenceType()
                    + (fn == null ? "" : "  " + fn.getName() + " " + fn.getEntryPoint()));
            if (from >= kBink) {
                out.println("    (inside BINK, not decompiled)");
                continue;
            }
            if (fn != null) {
                dumpFunction(fn);
            }
        }
        out.println();
    }

    private void dumpOne(long va) {
        out.println("== function " + Long.toHexString(va) + " ==");
        Function fn = getFunctionAt(toAddr(va));
        if (fn == null) {
            fn = getFunctionContaining(toAddr(va));
        }
        if (fn == null) {
            out.println("  no function");
            out.println();
            return;
        }
        dumpFunction(fn);
        out.println();
    }

    private void dumpFunction(Function fn) {
        if (fn == null) {
            return;
        }
        long va = fn.getEntryPoint().getOffset();
        if (va >= kBink) {
            out.println("-- skip BINK " + fn.getName() + " " + fn.getEntryPoint());
            return;
        }
        if (!dumped.add(va)) {
            return;
        }
        out.println("-- fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                + " size=" + fn.getBody().getNumAddresses());
        DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        out.println("callees:");
        Set<Function> called = fn.getCalledFunctions(monitor);
        if (called != null) {
            for (Function sub : called) {
                if (sub == null) {
                    continue;
                }
                long subVa = sub.getEntryPoint().getOffset();
                out.println("  " + sub.getName() + " " + sub.getEntryPoint() + (subVa >= kBink ? "  BINK" : ""));
            }
        }
        out.println();
    }
}
