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

public class ExportScript extends GhidraScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_script.txt"), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dumpString("There is no script for this scene!");
            dumpString("scriptConditionId_");
            dumpString("aid_script_");
            dumpString("aid_aidlist_ghoulies_sceneorder_game");
            dumpString("objMsgId_Scene_");
            dump("script_set_default_name", 0x1287F0L);
            dump("script_clear", 0x1289A0L);
            dumpAt("sceneorder_d8150", 0xD8150L);
            dumpAt("sceneorder_e50a0", 0xE50A0L);
            dumpAt("xref_d8159", 0xD8159L);
            dumpAt("xref_e50a9", 0xE50A9L);
            dumpAt("xref_e50f9", 0xE50F9L);
            dump("game_init_32630", 0x32630L);
            dump("frame_tick_33b50", 0x33B50L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_script.txt");
    }

    private void dumpString(String needle) throws Exception {
        out.println("== string \"" + needle + "\" ==");
        Memory mem = currentProgram.getMemory();
        byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
        var from = currentProgram.getMinAddress();
        int hits = 0;
        while (from != null && hits < 24) {
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
            while (refs.hasNext() && rc < 12) {
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
        if (fn == null) {
            dump(name, va);
        } else {
            dump(fn.getName(), fn.getEntryPoint().getOffset());
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
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 80) {
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
