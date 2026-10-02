// Headless dump of BanjoCam / entityBodyCamera from retail default.xbe.
//@category GbtG

import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

public class DumpCamera extends GhidraScript {
    private PrintWriter out;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_camera.txt"), true);
        try {
            out.println("format=" + currentProgram.getExecutableFormat());
            out.println("lang=" + currentProgram.getLanguageID());
            out.println("imageBase=" + currentProgram.getImageBase());
            out.println("name=" + currentProgram.getName());
            out.println();
            dumpBlocks();
            dumpSymbols();
            dumpString("entityBodyCamera");
            dumpString("BanjoCamAttachedToFocalActor");
            dumpString("objMsgId_Camera");
            dumpString("XInputGetState");

            decomp = new DecompInterface();
            DecompileOptions options = new DecompileOptions();
            options.grabFromProgram(currentProgram);
            decomp.setOptions(options);
            decomp.toggleCCode(true);
            if (!decomp.openProgram(currentProgram)) {
                out.println("decompiler: " + decomp.getLastMessage());
                return;
            }

            Function ctor = ensureFn(0x73190L, "entityBodyCamera_ctor");
            Function tick = ensureFn(0x731F0L, "entityBodyCamera_tick");
            dumpVtable(0x504F80L, 16);
            dumpInsns("tick", tick);
            decompile("ctor", ctor);
            decompile("tick", tick);
            decompile("vt0_73170", ensureFn(0x73170L, "cam_vt0"));
            decompile("vt2_73300", ensureFn(0x73300L, "cam_vt2"));
            decompile("vt3_73320", ensureFn(0x73320L, "cam_vt3"));
            dumpInsns("func_412a0", ensureFn(0x412a0L, "cam_func_412a0"));
            dumpInsns("func_41820", ensureFn(0x41820L, "cam_func_41820"));
            dumpInsns("func_48cf0", ensureFn(0x48cf0L, "cam_func_48cf0"));
            dumpInsns("func_496c0", ensureFn(0x496c0L, "cam_func_496c0"));
            decompile("func_412a0", getFunctionAt(toAddr(0x412a0L)));
            decompile("func_41820", getFunctionAt(toAddr(0x41820L)));
            decompile("func_48cf0", getFunctionAt(toAddr(0x48cf0L)));
            decompile("func_496c0", getFunctionAt(toAddr(0x496c0L)));
            dumpPad();
        } finally {
            if (decomp != null) {
                decomp.dispose();
            }
            out.close();
        }
        println("wrote cache/ghidra_camera.txt");
    }

    private void dumpBlocks() {
        out.println("== memory ==");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            out.printf("  %s %s-%s sz=0x%x r=%s w=%s x=%s%n", b.getName(), b.getStart(), b.getEnd(),
                    b.getSize(), b.isRead(), b.isWrite(), b.isExecute());
        }
        out.println();
    }

    private void dumpSymbols() {
        out.println("== symbols (camera / input / d3d) ==");
        SymbolTable st = currentProgram.getSymbolTable();
        SymbolIterator it = st.getAllSymbols(true);
        int n = 0;
        while (it.hasNext()) {
            Symbol s = it.next();
            String name = s.getName();
            String lower = name.toLowerCase();
            if (lower.contains("camera") || lower.contains("xinput") || lower.contains("banjo")
                    || lower.contains("settransform") || lower.contains("gettransform")
                    || name.contains("entityBody")) {
                out.println("  " + s.getAddress() + "  " + name);
                n++;
            }
        }
        out.println("  (" + n + " matches)");
        out.println();
    }

    private void dumpString(String needle) throws Exception {
        byte[] raw = (needle + "\0").getBytes(StandardCharsets.US_ASCII);
        Memory mem = currentProgram.getMemory();
        Address hit = mem.findBytes(currentProgram.getMinAddress(), raw, null, true, monitor);
        out.println("== string \"" + needle + "\" ==");
        if (hit == null) {
            out.println("  not found");
            out.println();
            return;
        }
        out.println("  at " + hit);
        byte[] ptr = new byte[] {(byte) hit.getOffset(), (byte) (hit.getOffset() >> 8),
                (byte) (hit.getOffset() >> 16), (byte) (hit.getOffset() >> 24)};
        Address from = currentProgram.getMinAddress();
        int n = 0;
        while (from != null && n < 32) {
            Address xref = mem.findBytes(from, ptr, null, true, monitor);
            if (xref == null) {
                break;
            }
            out.println("  ptr " + xref);
            from = xref.add(1);
            n++;
        }
        out.println();
    }

    private Function ensureFn(long va, String name) throws Exception {
        Address addr = toAddr(va);
        out.println("== ensure " + name + " @ " + addr + " ==");
        byte[] head = new byte[16];
        try {
            currentProgram.getMemory().getBytes(addr, head);
            out.print("  bytes");
            for (byte b : head) {
                out.printf(" %02x", b & 0xff);
            }
            out.println();
        } catch (Exception ex) {
            out.println("  unmapped: " + ex.getMessage());
            return null;
        }
        disassemble(addr);
        Function fn = getFunctionAt(addr);
        if (fn == null) {
            fn = createFunction(addr, name);
        } else {
            fn.setName(name, ghidra.program.model.symbol.SourceType.USER_DEFINED);
        }
        if (fn != null) {
            out.println("  body " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress());
        } else {
            out.println("  createFunction failed");
        }
        out.println();
        return fn;
    }

    private void dumpVtable(long va, int slots) throws Exception {
        Address addr = toAddr(va);
        out.println("== vtable @ " + addr + " ==");
        for (int i = 0; i < slots; i++) {
            long slot = currentProgram.getMemory().getInt(addr.add(i * 4L)) & 0xffffffffL;
            out.printf("  [%02d] %08x%n", i, slot);
        }
        out.println();
    }

    private void dumpPad() throws Exception {
        Address handles = toAddr(0x4348A0L);
        out.println("== pad handles @ " + handles + " ==");
        try {
            for (int i = 0; i < 4; i++) {
                out.printf("  [%d] %08x%n", i, currentProgram.getMemory().getInt(handles.add(i * 4L)));
            }
        } catch (Exception ex) {
            out.println("  " + ex.getMessage());
        }
        Function xinput = getFunction("XInputGetState");
        if (xinput == null) {
            Address a = toAddr(0x1FDC68L);
            xinput = ensureFn(0x1FDC68L, "XInputGetState");
            if (xinput == null && getInstructionAt(a) != null) {
                xinput = getFunctionContaining(a);
            }
        }
        decompile("XInputGetState", xinput);
        out.println();
    }

    private void dumpInsns(String tag, Function fn) {
        out.println("== insns " + tag + " ==");
        if (fn == null) {
            out.println("  null");
            out.println();
            return;
        }
        int n = 0;
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        while (ins != null && fn.getBody().contains(ins.getAddress()) && n < 80) {
            out.println("  " + ins.getAddress() + "  " + ins);
            ins = ins.getNext();
            n++;
        }
        if (ins != null && fn.getBody().contains(ins.getAddress())) {
            out.println("  ...");
        }
        out.println();
    }

    private void decompile(String tag, Function fn) {
        out.println("== decompile " + tag + " ==");
        if (fn == null) {
            out.println("  null");
            out.println();
            return;
        }
        Instruction last = getInstructionAt(fn.getBody().getMaxAddress());
        if (last != null) {
            out.println("  last " + last.getAddress() + " " + last);
        }
        DecompileResults res = decomp.decompileFunction(fn, 60, monitor);
        if (res == null || res.getDecompiledFunction() == null) {
            out.println("  failed: " + (res == null ? "null" : res.getErrorMessage()));
            out.println();
            return;
        }
        out.println(res.getDecompiledFunction().getC());
        out.println();
    }
}
