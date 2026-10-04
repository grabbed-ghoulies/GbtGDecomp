//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpApply extends GhidraScript {

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

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter(cacheFile("ghidra_apply.txt")), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        long[] vas = {0x25c50L, 0x25190L, 0x412a0L, 0x41820L};
        try {
            for (long va : vas) {
                var addr = toAddr(va);
                out.println("== " + addr + " ==");
                disassemble(addr);
                Function fn = getFunctionContaining(addr);
                if (fn == null) {
                    fn = createFunction(addr, "fn_" + Long.toHexString(va));
                }
                if (fn == null) {
                    out.println("no function");
                    continue;
                }
                out.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress());
                Instruction ins = getInstructionAt(fn.getEntryPoint());
                int n = 0;
                while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 80) {
                    out.println("  " + ins.getAddress() + "  " + ins);
                    ins = ins.getNext();
                    n++;
                }
                DecompileResults res = decomp.decompileFunction(fn, 90, monitor);
                if (res != null && res.getDecompiledFunction() != null) {
                    out.println(res.getDecompiledFunction().getC());
                } else {
                    out.println("decompile failed");
                }
                out.println();
            }
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_apply.txt");
    }
}
