//@category GbtG
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportUi extends GhidraScript {
    private PrintWriter out;
    private DecompInterface decomp;
    private final HashSet<String> dumped = new HashSet<>();

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter("E:/GhouliesPort/GbtGDecomp/cache/ghidra_ui.txt"), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            dumped.clear();
            dump(0x58c30L);
            dump(0x5f5c0L);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_ui.txt");
    }

    private void dumpString(String needle) throws Exception {
        out.println("== string \"" + needle + "\" ==");
        Memory mem = currentProgram.getMemory();
        byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
        Address from = currentProgram.getMinAddress();
        int hits = 0;
        while (from != null && hits < 6) {
            Address hit = mem.findBytes(from, raw, null, true, monitor);
            if (hit == null) {
                break;
            }
            out.println("  str " + hit);
            byte[] before = new byte[48];
            Address pre = hit.subtract(48);
            mem.getBytes(pre, before);
            out.print("  pre ");
            for (byte b : before) {
                out.printf("%02x ", b & 0xff);
            }
            out.println();
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(hit);
            int rc = 0;
            while (refs.hasNext() && rc < 12) {
                Reference r = refs.next();
                Function fn = getFunctionContaining(r.getFromAddress());
                String name = fn == null ? "" : fn.getName() + " " + fn.getEntryPoint();
                out.println("    xref " + r.getFromAddress() + " " + name);
                if (fn != null) {
                    decompile(fn);
                }
                rc++;
            }
            from = hit.add(1);
            hits++;
        }
        out.println();
    }

    private void listing(long va) {
        Function fn = getFunctionContaining(toAddr(va));
        out.println("== listing " + (fn == null ? "?" : fn.getName()) + " ==");
        if (fn == null) {
            return;
        }
        Instruction ins = getInstructionAt(fn.getEntryPoint());
        while (ins != null && fn.getBody().contains(ins.getAddress())) {
            String s = ins.toString();
            String addr = ins.getAddress().toString();
            if (addr.startsWith("000dff") || addr.startsWith("000e00") || s.contains("0042904") || s.contains("0042903")) {
                out.println("  " + ins.getAddress() + "  " + s);
            }
            ins = ins.getNext();
        }
        out.println();
    }

    private void dump(long va) {
        Address addr = toAddr(va);
        Function fn = getFunctionContaining(addr);
        if (fn == null) {
            fn = getFunctionAt(addr);
        }
        out.println("== va " + addr + " fn " + (fn == null ? "none" : fn.getName() + " " + fn.getEntryPoint()) + " ==");
        if (fn != null) {
            dumped.remove(fn.getEntryPoint().toString());
            decompile(fn);
        }
        out.println();
    }

    private void xrefsTo(long va) {
        Address addr = toAddr(va);
        out.println("== xrefs " + addr + " ==");
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        int rc = 0;
        while (refs.hasNext() && rc < 16) {
            Reference r = refs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  " + r.getFromAddress() + " " + (fn == null ? "" : fn.getName()));
            rc++;
        }
        if (rc == 0) {
            out.println("  none");
        }
        out.println();
    }

    private void findOps(String needle, int cap) throws Exception {
        out.println("== ops " + needle + " ==");
        ghidra.program.model.listing.InstructionIterator it = currentProgram.getListing().getInstructions(true);
        int n = 0;
        while (it.hasNext() && n < cap) {
            Instruction ins = it.next();
            String s = ins.toString();
            if (s.contains(needle)) {
                Function fn = getFunctionContaining(ins.getAddress());
                out.println("  " + ins.getAddress() + "  " + s + "  " + (fn == null ? "" : fn.getName()));
                n++;
            }
        }
        out.println("  count " + n);
        out.println();
    }

    private void listingRange(long start, long end) {
        out.println("== range " + Long.toHexString(start) + " ==");
        Instruction ins = getInstructionAt(toAddr(start));
        if (ins == null) {
            ins = getInstructionAfter(toAddr(start));
        }
        Address stop = toAddr(end);
        while (ins != null && ins.getAddress().compareTo(stop) < 0) {
            out.println("  " + ins.getAddress() + "  " + ins.toString());
            ins = ins.getNext();
        }
        out.println();
    }

    private void decompile(Function fn) {
        String key = fn.getEntryPoint().toString();
        if (!dumped.add(key) || dumped.size() > 40) {
            return;
        }
        out.println("---- " + fn.getName() + " " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses());
        DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            String c = res.getDecompiledFunction().getC();
            if (c.length() > 12000) {
                c = c.substring(0, 12000) + "\n...truncated...\n";
            }
            out.println(c);
        } else {
            out.println("decompile failed");
        }
    }
}
