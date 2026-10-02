//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class ExportWinMain extends GhidraScript {
    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_winmain.txt"), true);
        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            var addr = toAddr(0x32390L);
            disassemble(addr);
            Function fn = getFunctionContaining(addr);
            if (fn == null) {
                fn = createFunction(addr, "WinMain");
            } else {
                fn.setName("WinMain", ghidra.program.model.symbol.SourceType.USER_DEFINED);
            }
            out.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                    + " size=" + fn.getBody().getNumAddresses());
            Instruction ins = getInstructionAt(fn.getEntryPoint());
            int n = 0;
            while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 80) {
                out.println("  " + ins.getAddress() + "  " + ins);
                ins = ins.getNext();
                n++;
            }
            DecompileResults res = decomp.decompileFunction(fn, 180, monitor);
            if (res != null && res.getDecompiledFunction() != null) {
                out.println(res.getDecompiledFunction().getC());
            } else {
                out.println("decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
            }
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_winmain.txt");
    }
}
