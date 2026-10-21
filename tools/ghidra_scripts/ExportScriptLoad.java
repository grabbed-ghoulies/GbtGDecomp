//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;

public class ExportScriptLoad extends GhidraScript {

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

    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter(cacheFile("ghidra_scriptload.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dump("script_set_name_128ab0", 0x128AB0L);
            dump("script_reset_127b50", 0x127B50L);
            dump("aidlist_load_1234b0", 0x1234B0L);
            dump("io_remain_e9040", 0xE9040L);
            dump("io_complete_e9280", 0xE9280L);
            dump("clock_init_fad20", 0xFAD20L);
            dump("pad_more_fa210", 0xFA210L);
            dump("pad_fa3c0", 0xFA3C0L);
            dump("pad_fad80", 0xFAD80L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_scriptload.txt");
    }

    private void dump(String name, long va) throws Exception {
        var addr = toAddr(va);
        disassemble(addr);
        Function fn = getFunctionContaining(addr);
        if (fn == null) {
            fn = createFunction(addr, name);
        } else {
            try {
                fn.setName(name, SourceType.USER_DEFINED);
            } catch (Exception e) {
            }
        }
        out.println("== " + name + " @ " + addr + " ==");
        if (fn == null) {
            out.println("no function");
            out.println();
            return;
        }
        out.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                + " size=" + fn.getBody().getNumAddresses());
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        int n = 0;
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 90) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        DecompileResults res = decomp.decompileFunction(fn, 240, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("decompile failed");
        }
        out.println();
    }
}
