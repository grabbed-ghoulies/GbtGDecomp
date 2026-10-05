//@category GbtG
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class ExportMovie4 extends GhidraScript {
    @Override
    public void run() throws Exception {
        File root = new File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && root != null; i++) {
            if (new File(root, "CMakeLists.txt").isFile()) {
                break;
            }
            root = root.getParentFile();
        }
        PrintWriter out = new PrintWriter(new FileWriter(new File(root, "cache/ghidra_movie4.txt")), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            for (long va : new long[] {0x106CD0L, 0x106C70L, 0xD9640L, 0xE62F0L, 0xDD740L}) {
                Function fn = getFunctionAt(toAddr(va));
                out.println("== " + Long.toHexString(va) + " ==");
                if (fn == null) {
                    out.println("none");
                    continue;
                }
                DecompileResults res = decomp.decompileFunction(fn, 90, monitor);
                if (res != null && res.getDecompiledFunction() != null) {
                    out.println(res.getDecompiledFunction().getC());
                }
            }
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_movie4.txt");
    }
}
