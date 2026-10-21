//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.SourceType;

public class ExportNext extends GhidraScript {

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
        out = new PrintWriter(new FileWriter(cacheFile("ghidra_next.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dumpString("aid_aidlist_ghoulies_sceneorder_game");
            dumpString("sceneorder");
            dumpString("There is no script for this scene!");
            dump("file_pump_e9e60", 0xE9E60L);
            dump("game_init_wait_39d50", 0x39D50L);
            dump("fs_more_33030", 0x33030L);
            dump("frame_time_33c80", 0x33C80L);
            dump("pad_fc030", 0xFC030L);
            dump("sys_3a040", 0x3A040L);
            dump("sys_396f0", 0x396F0L);
            dump("sys_38640", 0x38640L);
            dump("sys_38c20", 0x38C20L);
            dump("audio_15b780", 0x15B780L);
            dump("present_33480", 0x33480L);
            dump("present_32ab0", 0x32AB0L);
            dump("present_33a90", 0x33A90L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_next.txt");
    }

    private void dumpString(String needle) throws Exception {
        out.println("== string \"" + needle + "\" ==");
        Memory mem = currentProgram.getMemory();
        byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
        Address from = currentProgram.getMinAddress();
        int hits = 0;
        while (from != null && hits < 8) {
            Address hit = mem.findBytes(from, raw, null, true, monitor);
            if (hit == null) {
                break;
            }
            out.println("  str " + hit);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(hit);
            int rc = 0;
            while (refs.hasNext() && rc < 16) {
                Reference r = refs.next();
                Function fn = getFunctionContaining(r.getFromAddress());
                out.println("    xref " + r.getFromAddress() + (fn == null ? "" : "  " + fn.getName()));
                rc++;
            }
            from = hit.add(1);
            hits++;
        }
        out.println();
    }

    private void dump(String name, long va) throws Exception {
        var addr = toAddr(va);
        disassemble(addr);
        Function fn = getFunctionContaining(addr);
        if (fn == null) {
            fn = createFunction(addr, name);
        } else {
            fn.setName(name, SourceType.USER_DEFINED);
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
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 48) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        DecompileResults res = decomp.decompileFunction(fn, 180, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("decompile failed");
        }
        out.println();
    }
}
