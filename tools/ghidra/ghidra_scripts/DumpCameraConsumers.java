//@category GbtG

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpCameraConsumers extends GhidraScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_cam_consumers.txt"), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        if (!decomp.openProgram(currentProgram)) {
            out.println(decomp.getLastMessage());
            out.close();
            return;
        }
        try {
            long[] vas = {0x72ac0L, 0x72b10L, 0x72cb0L, 0x73400L, 0x734b0L, 0x75300L, 0x756c0L,
                    0x757c0L, 0x758a0L, 0x11c480L, 0x1555a0L, 0x155624L};
            for (long va : vas) {
                Function fn = ensureFn(va);
                dumpInsns(fn);
                decompile(fn);
            }
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_cam_consumers.txt");
    }

    private Function ensureFn(long va) throws Exception {
        var addr = toAddr(va);
        out.println("== " + addr + " ==");
        byte[] head = new byte[12];
        currentProgram.getMemory().getBytes(addr, head);
        out.print("  bytes");
        for (byte b : head) {
            out.printf(" %02x", b & 0xff);
        }
        out.println();
        disassemble(addr);
        Function fn = getFunctionContaining(addr);
        if (fn == null) {
            fn = createFunction(addr, "cam_" + Long.toHexString(va));
        }
        if (fn != null) {
            out.println("  fn " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress() + " " + fn.getName());
        }
        return fn;
    }

    private void dumpInsns(Function fn) {
        if (fn == null) {
            return;
        }
        int n = 0;
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 60) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        out.println();
    }

    private void decompile(Function fn) {
        if (fn == null) {
            out.println("  null fn\n");
            return;
        }
        DecompileResults res = decomp.decompileFunction(fn, 60, monitor);
        if (res == null || res.getDecompiledFunction() == null) {
            out.println("  decomp failed\n");
            return;
        }
        out.println(res.getDecompiledFunction().getC());
        out.println();
    }
}
