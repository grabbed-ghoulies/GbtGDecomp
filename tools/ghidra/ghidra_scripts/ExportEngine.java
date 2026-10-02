// Dump recovered engine map from the analyzed GbtG Ghidra project.
//@category GbtG

import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

public class ExportEngine extends GbtgScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        String path = cacheFile("ghidra_engine.txt").getPath();
        String mapPath = repoFile("docs/generated/ghidra_functions.txt").getPath();
        out = new PrintWriter(new FileWriter(path), true);
        PrintWriter map = new PrintWriter(new FileWriter(mapPath), true);
        try {
            out.println("format=" + currentProgram.getExecutableFormat());
            out.println("lang=" + currentProgram.getLanguageID());
            out.println("imageBase=" + currentProgram.getImageBase());
            out.println("name=" + currentProgram.getName());
            out.println("entry=" + currentProgram.getAddressFactory().getDefaultAddressSpace()
                    .getAddress(currentProgram.getSymbolTable().getExternalEntryPointIterator().hasNext()
                            ? currentProgram.getSymbolTable().getExternalEntryPointIterator().next().getOffset()
                            : 0));
            out.println();

            decomp = new DecompInterface();
            DecompileOptions options = new DecompileOptions();
            options.grabFromProgram(currentProgram);
            decomp.setOptions(options);
            decomp.toggleCCode(true);
            if (!decomp.openProgram(currentProgram)) {
                out.println("decompiler: " + decomp.getLastMessage());
                return;
            }

            dumpUserSymbols(map);
            dumpNamedFunctions(map);
            dumpStringXrefs();
            decompileAt("entry_14FAE9", 0x14FAE9L);
            decompileAt("entityType_register_1DA60", 0x1DA60L);
            dumpXrefsTo(0x1DA60L, "entityType_register");
            decompileAt("entityBodyCamera_ctor", 0x73190L);
            decompileAt("entityBodyCamera_tick", 0x731F0L);
            decompileAt("entityBodyCamera_sizeof", 0x73170L);
            decompileAt("entityBodyCamera_shutdown", 0x73300L);
            decompileAt("entityBodyCamera_msg", 0x73320L);
            decompileAt("actor_world_pos_412a0", 0x412A0L);
            decompileAt("actor_world_euler_41820", 0x41820L);
            decompileAt("camera_apply_25ae0", 0x25AE0L);
            decompileAt("banjocam_lookat_72ba0", 0x72BA0L);
        } finally {
            if (decomp != null) {
                decomp.dispose();
            }
            out.close();
            map.close();
        }
        println("wrote " + path + " and " + mapPath);
    }

    private void dumpUserSymbols(PrintWriter map) {
        out.println("== user symbols ==");
        map.println("# Ghidra function / symbol names (addresses only; no binary).");
        map.println("# va          source          name");
        SymbolTable st = currentProgram.getSymbolTable();
        SymbolIterator it = st.getAllSymbols(true);
        TreeMap<Long, String> rows = new TreeMap<Long, String>();
        int n = 0;
        while (it.hasNext()) {
            Symbol s = it.next();
            SourceType src = s.getSource();
            if (src != SourceType.USER_DEFINED && src != SourceType.IMPORTED && src != SourceType.ANALYSIS) {
                continue;
            }
            String name = s.getName();
            if (src == SourceType.ANALYSIS && (name.startsWith("FUN_") || name.startsWith("DAT_")
                    || name.startsWith("PTR_") || name.startsWith("s_") || name.startsWith("u_")
                    || name.startsWith("LAB_") || name.startsWith("SUB_"))) {
                continue;
            }
            long va = s.getAddress().getOffset();
            String line = String.format("%08x  %-14s  %s", va, src.toString(), name);
            if (src == SourceType.USER_DEFINED || interestingName(name)) {
                rows.put(va, line);
                n++;
            }
        }
        for (String line : rows.values()) {
            out.println("  " + line);
            map.println(line);
        }
        out.println("  (" + n + " kept)");
        out.println();
        map.println();
    }

    private boolean interestingName(String name) {
        String l = name.toLowerCase();
        return l.contains("entity") || l.contains("actor") || l.contains("camera") || l.contains("script")
                || l.contains("audio") || l.contains("bundle") || l.contains("banjo") || name.contains("WinMain")
                || name.contains("mainCRT") || l.startsWith("objmsg") || l.contains("aid_");
    }

    private void dumpNamedFunctions(PrintWriter map) {
        out.println("== functions ==");
        map.println("# functions");
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int total = 0;
        int named = 0;
        while (it.hasNext()) {
            Function fn = it.next();
            total++;
            String name = fn.getName();
            if (name.startsWith("FUN_") || name.startsWith("thunk_FUN_")) {
                continue;
            }
            named++;
            long va = fn.getEntryPoint().getOffset();
            String line = String.format("%08x  %-8d  %s", va, fn.getBody().getNumAddresses(), name);
            out.println("  " + line);
            if (fn.getSymbol() != null && fn.getSymbol().getSource() == SourceType.USER_DEFINED) {
                map.println(line);
            }
        }
        out.println("  named=" + named + " total=" + total);
        out.println();
        map.println("# named=" + named + " total=" + total);
        map.println();
    }

    private void dumpStringXrefs() throws Exception {
        String[] needles = {
            "entityBody", "entityMind", "entityStrategy", "actorMindGoalId_", "actorStrategyId_",
            "objMsgId_", "scriptConditionId_", "audioLoadFile", "bundles\\", "demand\\",
            "There is no script for this scene!", "ndVertexBuffer", "WinMain"
        };
        out.println("== string xrefs ==");
        Memory mem = currentProgram.getMemory();
        for (String needle : needles) {
            out.println("-- " + needle + " --");
            byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
            Address from = currentProgram.getMinAddress();
            int hits = 0;
            while (from != null && hits < 24) {
                Address hit = mem.findBytes(from, raw, null, true, monitor);
                if (hit == null) {
                    break;
                }
                out.println("  str " + hit);
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
        }
        out.println();
    }

    private void dumpXrefsTo(long va, String tag) {
        out.println("== xrefs to " + tag + " @ " + Long.toHexString(va) + " ==");
        Address addr = toAddr(va);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        List<Long> callers = new ArrayList<Long>();
        while (refs.hasNext()) {
            Reference r = refs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + (fn == null ? "" : "  " + fn.getName()));
            if (fn != null) {
                callers.add(fn.getEntryPoint().getOffset());
            }
        }
        out.println();
        int n = 0;
        for (Long c : callers) {
            if (n++ >= 40) {
                out.println("  ... more type ctors omitted");
                break;
            }
            Function fn = getFunctionAt(toAddr(c));
            decompile("typector_" + Long.toHexString(c), fn);
        }
    }

    private void decompileAt(String tag, long va) {
        Address addr = toAddr(va);
        disassemble(addr);
        Function fn = getFunctionAt(addr);
        if (fn == null) {
            fn = getFunctionContaining(addr);
        }
        if (fn == null) {
            try {
                fn = createFunction(addr, tag);
            } catch (Exception ex) {
                out.println("== decompile " + tag + " ==");
                out.println("  create failed: " + ex.getMessage());
                out.println();
                return;
            }
        }
        decompile(tag, fn);
    }

    private void decompile(String tag, Function fn) {
        out.println("== decompile " + tag + " ==");
        if (fn == null) {
            out.println("  null");
            out.println();
            return;
        }
        out.println("  fn " + fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()
                + " size=" + fn.getBody().getNumAddresses());
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        int n = 0;
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 24) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        DecompileResults res = decomp.decompileFunction(fn, 90, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        } else {
            out.println("  decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        out.println();
    }
}
