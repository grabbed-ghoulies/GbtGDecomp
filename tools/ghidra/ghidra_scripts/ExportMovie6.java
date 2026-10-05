// Read-only dump of the cutscene path that opens movies.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ExportMovie6 extends GhidraScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        File root = new File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && root != null; i++) {
            if (new File(root, "CMakeLists.txt").isFile()) {
                break;
            }
            root = root.getParentFile();
        }
        out = new PrintWriter(new FileWriter(new File(root, "cache/ghidra_movie6.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        long[] vas = {0x4C6C0L, 0x1BB60L};
        out.println("== xrefs 4d370 ==");
        ghidra.program.model.symbol.ReferenceIterator erefs = currentProgram.getReferenceManager().getReferencesTo(toAddr(0x4D370L));
        while (erefs.hasNext()) {
            ghidra.program.model.symbol.Reference r = erefs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + (fn == null ? "" : " " + fn.getName() + " " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses()));
        }
        Address table = toAddr(0x545DC0L);
        out.println("== table 545dc0 ==");
        for (int i = 0; i < 40; i++) {
            Address slot = table.add(i * 0x14);
            long fn = currentProgram.getMemory().getInt(slot) & 0xffffffffL;
            long a = currentProgram.getMemory().getInt(slot.add(4)) & 0xffffffffL;
            long b = currentProgram.getMemory().getInt(slot.add(8)) & 0xffffffffL;
            Function cb = getFunctionAt(toAddr(fn));
            out.println(String.format("  [%d] fn=%08x %s a=%08x b=%08x", i, fn, cb == null ? "" : cb.getName(), a, b));
        }
        out.println("== xrefs 545dc0 ==");
        ghidra.program.model.symbol.ReferenceIterator trefs = currentProgram.getReferenceManager().getReferencesTo(table);
        while (trefs.hasNext()) {
            ghidra.program.model.symbol.Reference r = trefs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + " " + r.getReferenceType() + (fn == null ? "" : " " + fn.getName() + " " + fn.getEntryPoint()));
        }
        out.println("== xrefs 4c850 ==");
        ghidra.program.model.symbol.ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(toAddr(0x4C850L));
        while (refs.hasNext()) {
            ghidra.program.model.symbol.Reference r = refs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + (fn == null ? "" : " " + fn.getName() + " " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses()));
        }
        out.println("== listing 106d50 ==");
        ghidra.program.model.listing.InstructionIterator it = currentProgram.getListing().getInstructions(getFunctionAt(toAddr(0x106D50L)).getBody(), true);
        while (it.hasNext()) {
            ghidra.program.model.listing.Instruction ins = it.next();
            out.println("  " + ins.getAddress() + "  " + ins);
        }
        for (long va : vas) {
            Function fn = getFunctionAt(toAddr(va));
            if (fn == null) {
                fn = getFunctionContaining(toAddr(va));
            }
            out.println("== " + Long.toHexString(va) + " ==");
            if (fn == null) {
                out.println("none");
                continue;
            }
            out.println(fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                    + " size=" + fn.getBody().getNumAddresses());
            DecompileResults res = decomp.decompileFunction(fn, 180, monitor);
            if (res != null && res.getDecompiledFunction() != null) {
                out.println(res.getDecompiledFunction().getC());
            } else {
                out.println("fail " + (res == null ? "null" : res.getErrorMessage()));
            }
            out.println();
        }
        decomp.dispose();
        out.close();
        println("wrote cache/ghidra_movie6.txt");
    }
}
