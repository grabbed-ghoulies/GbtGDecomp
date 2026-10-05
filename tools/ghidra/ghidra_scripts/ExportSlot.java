//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class ExportSlot extends GhidraScript {

    private java.io.File repoRoot() {
        java.io.File dir = sourceFile == null ? null : new java.io.File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new java.io.File(dir, "CMakeLists.txt").isFile()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        return new java.io.File(System.getProperty("user.dir"));
    }

    @Override
    public void run() throws Exception {
        java.io.File outFile = new java.io.File(repoRoot(), "cache/ghidra_slot.txt");
        PrintWriter out = new PrintWriter(new FileWriter(outFile), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            long[] vas = {
                0xDF9A0L, 0x5F5C0L
            };
            for (long va : vas) {
                Address addr = toAddr(va);
                Function fn = getFunctionContaining(addr);
                if (fn == null) {
                    fn = getFunctionAt(addr);
                }
                out.println("== va " + addr + " fn " + (fn == null ? "none" : fn.getName() + " " + fn.getEntryPoint()) + " ==");
                if (fn == null) {
                    continue;
                }
                out.println("---- " + fn.getName() + " " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses());
                DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
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
        println("wrote cache/ghidra_slot.txt");
    }
}
