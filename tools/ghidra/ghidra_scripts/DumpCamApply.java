//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpCamApply extends GhidraScript {
    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_cam_apply.txt"), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        long[] vas = {0x25ae0L, 0x426f0L, 0x12b160L, 0x72ba0L};
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
                while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 120) {
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
        println("wrote cache/ghidra_cam_apply.txt");
    }
}
