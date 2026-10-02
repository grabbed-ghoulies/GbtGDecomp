//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.SourceType;

public class ExportCutscene extends GbtgScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter(cacheFile("ghidra_cutscene.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dumpString("aid_cutscene_");
            dumpString("objMsgId_Scene_Cutscene");
            dumpString("objMsgId_Scene_Begin");
            dumpString("objMsgId_Scene_End");
            dumpString("objMsgId_Scene_EventBegin");
            dumpString("cutscene");
            dumpAt("cutscene_alloc_msg", 0x327B78L); // string area; resolve via xref
            dump("script_set_name_128ab0", 0x128AB0L);
            dump("fun_12af10", 0x12AF10L);
            dump("fun_117400", 0x117400L);
            dump("fun_35e90", 0x35E90L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_cutscene.txt");
    }

    private void dumpString(String needle) throws Exception {
        out.println("== string \"" + needle + "\" ==");
        Memory mem = currentProgram.getMemory();
        byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
        var from = currentProgram.getMinAddress();
        int hits = 0;
        while (from != null && hits < 16) {
            var hit = mem.findBytes(from, raw, null, true, monitor);
            if (hit == null) {
                break;
            }
            byte[] buf = new byte[96];
            try {
                mem.getBytes(hit, buf);
            } catch (Exception e) {
                buf = raw;
            }
            int n = 0;
            while (n < buf.length && buf[n] != 0) {
                n++;
            }
            String s = new String(buf, 0, n, StandardCharsets.US_ASCII);
            out.println("  str " + hit + "  " + s);
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(hit);
            int rc = 0;
            while (refs.hasNext() && rc < 8) {
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

    private void dumpAt(String name, long va) throws Exception {
        Function fn = getFunctionContaining(toAddr(va));
        if (fn != null) {
            dump(fn.getName(), fn.getEntryPoint().getOffset());
        } else {
            dump(name, va);
        }
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
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 70) {
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
