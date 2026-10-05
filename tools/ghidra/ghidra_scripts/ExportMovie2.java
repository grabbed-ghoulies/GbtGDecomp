// Callers of the movie allocator, and every .text call into the BINK range.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.LinkedHashSet;
import java.util.Set;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportMovie2 extends GhidraScript {

    private static final long kBink = 0x1A59E0L;
    private static final long kBinkEnd = 0x1BABA0L;
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
        return new File(System.getProperty("user.dir"));
    }

    @Override
    public void run() throws Exception {
        File cache = new File(repoRoot(), "cache");
        if (!cache.isDirectory() && !cache.mkdirs()) {
            throw new IllegalStateException("could not create " + cache.getAbsolutePath());
        }
        out = new PrintWriter(new FileWriter(new File(cache, "ghidra_movie2.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            for (long va : new long[] {0x1071A0L, 0x107030L, 0x107420L, 0x106EF0L, 0x107400L, 0x1074A0L, 0x106FE0L, 0x1073E0L}) {
                dumpAsm(va);
            }
            dumpXrefs(0x1071A0L, "movie_alloc");
            dumpXrefs(0x107030L, "movie_open");
            dumpXrefs(0x107400L, "movie_free_cb");
            dumpXrefs(0x1074A0L, "movie_1074a0");
            dumpXrefs(0x358B0L, "hash_358b0");
            out.println("== .text calls into BINK ==");
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext()) {
                Function fn = it.next();
                long entry = fn.getEntryPoint().getOffset();
                if (entry >= kBink) {
                    break;
                }
                Set<Function> called = fn.getCalledFunctions(monitor);
                if (called == null) {
                    continue;
                }
                for (Function sub : called) {
                    if (sub == null) {
                        continue;
                    }
                    long subVa = sub.getEntryPoint().getOffset();
                    if (subVa >= kBink && subVa < kBinkEnd) {
                        out.println("  " + fn.getName() + " " + fn.getEntryPoint() + " -> " + sub.getName() + " " + sub.getEntryPoint());
                        dumpFunction(fn);
                    }
                }
            }
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_movie2.txt");
    }

    private void dumpAsm(long va) {
        Function fn = getFunctionAt(toAddr(va));
        if (fn == null) {
            fn = getFunctionContaining(toAddr(va));
        }
        out.println("== asm " + Long.toHexString(va) + " ==");
        if (fn == null) {
            out.println("  no function");
            return;
        }
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        int n = 0;
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 160) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        out.println();
    }

    private void dumpXrefs(long va, String tag) {
        out.println("== xrefs " + tag + " ==");
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(toAddr(va));
        while (refs.hasNext()) {
            Reference r = refs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + " " + r.getReferenceType()
                    + (fn == null ? "" : "  " + fn.getName() + " " + fn.getEntryPoint()));
            if (fn != null && fn.getEntryPoint().getOffset() < kBink) {
                dumpFunction(fn);
            }
        }
        out.println();
    }

    private void dumpFunction(Function fn) {
        long va = fn.getEntryPoint().getOffset();
        if (va >= kBink || !dumped.add(va)) {
            return;
        }
        out.println("-- fn " + fn.getName() + " " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses());
        DecompileResults res = decomp.decompileFunction(fn, 90, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("decompile failed");
        }
        out.println();
    }
}
