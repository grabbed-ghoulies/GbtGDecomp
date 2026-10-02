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

public class ExportGameLoop extends GbtgScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter(cacheFile("ghidra_gameloop.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dump("gfx_init_32470", 0x32470L);
            dump("fs_init_33980", 0x33980L);
            dump("game_init_32630", 0x32630L);
            dump("frame_tick_33b50", 0x33B50L);
            dump("movie_cb_3a360", 0x3A360L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_gameloop.txt");
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
        out.println("== " + name + " ==");
        if (fn == null) {
            out.println("no function");
            out.println();
            return;
        }
        out.println("fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                + " size=" + fn.getBody().getNumAddresses());
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        int n = 0;
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 60) {
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
