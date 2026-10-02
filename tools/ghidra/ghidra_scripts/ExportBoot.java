//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class ExportBoot extends GhidraScript {
    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_boot.txt"), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        long[] vas = {0x14FA75L, 0x33E00L, 0x33FB0L, 0x34290L, 0x34FAAL, 0x1287F0L, 0x1289A0L, 0x15BB30L};
        String[] names = {"xapi_thread", "bundle_path_33e00", "bundle_path_33fb0", "bundle_path_34290",
                "demand_path_34faa", "script_missing_1287f0", "script_missing_1289a0", "audioLoadFile_15bb30"};
        try {
            for (int i = 0; i < vas.length; i++) {
                var addr = toAddr(vas[i]);
                out.println("== " + names[i] + " @ " + addr + " ==");
                disassemble(addr);
                Function fn = getFunctionContaining(addr);
                if (fn == null) {
                    fn = createFunction(addr, names[i]);
                }
                if (fn == null) {
                    out.println("no function");
                    continue;
                }
                out.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                        + " size=" + fn.getBody().getNumAddresses());
                Instruction ins = getInstructionAt(fn.getEntryPoint());
                int n = 0;
                while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 40) {
                    out.println("  " + ins.getAddress() + "  " + ins);
                    ins = ins.getNext();
                    n++;
                }
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
        println("wrote cache/ghidra_boot.txt");
    }
}
