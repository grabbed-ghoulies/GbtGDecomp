// Movie blit callers, async file helpers, and the gameselect name tables.
//@category GbtG

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportMovie3 extends GhidraScript {

    private PrintWriter out;
    private DecompInterface decomp;

    private File repoRoot() {
        File dir = sourceFile == null ? null : new File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new File(dir, "CMakeLists.txt").isFile()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        return new File(System.getProperty("user.dir"));
    }

    @Override
    public void run() throws Exception {
        out = new PrintWriter(new FileWriter(new File(repoRoot(), "cache/ghidra_movie3.txt")), true);
        decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        try {
            for (long va : new long[] {0x107360L, 0x1072B0L, 0xE01B0L, 0xE87F0L, 0x106D50L, 0xEAA90L, 0xEAED0L, 0xEB310L, 0xEB0E0L, 0x1074A0L, 0x107400L, 0xE96A0L, 0x32700L, 0x358B0L, 0x35830L, 0x357D0L}) {
                xref(va);
            }
            dumpCString(0x4290C8L, 0x124, 8);
            dumpCString(0x42941CL, 0x10C, 8);
            dumpBytes(0x106FE0L, 64);
        } finally {
            decomp.dispose();
            out.close();
        }
        println("wrote cache/ghidra_movie3.txt");
    }

    private void xref(long va) {
        out.println("== " + Long.toHexString(va) + " ==");
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(toAddr(va));
        while (refs.hasNext()) {
            Reference r = refs.next();
            Function fn = getFunctionContaining(r.getFromAddress());
            out.println("  xref " + r.getFromAddress() + (fn == null ? "" : " " + fn.getName() + " " + fn.getEntryPoint()));
        }
        Function fn = getFunctionAt(toAddr(va));
        if (fn == null) {
            out.println("  no function");
            out.println();
            return;
        }
        DecompileResults res = decomp.decompileFunction(fn, 60, monitor);
        if (res != null && res.getDecompiledFunction() != null) {
            out.println(res.getDecompiledFunction().getC());
        }
        out.println();
    }

    private void dumpCString(long va, int stride, int count) throws Exception {
        out.println("== strings " + Long.toHexString(va) + " stride " + Integer.toHexString(stride) + " ==");
        Memory mem = currentProgram.getMemory();
        for (int i = 0; i < count; i++) {
            byte[] buf = new byte[96];
            mem.getBytes(toAddr(va + (long) i * stride), buf);
            int n = 0;
            while (n < buf.length && buf[n] != 0) {
                n++;
            }
            out.println("  [" + i + "] " + new String(buf, 0, n, java.nio.charset.StandardCharsets.US_ASCII));
        }
        out.println();
    }

    private void dumpBytes(long va, int n) throws Exception {
        out.println("== bytes " + Long.toHexString(va) + " ==");
        byte[] buf = new byte[n];
        currentProgram.getMemory().getBytes(toAddr(va), buf);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) {
            if (i % 16 == 0) {
                if (i > 0) {
                    out.println(sb.toString());
                    sb.setLength(0);
                }
                sb.append(String.format("%08x  ", va + i));
            }
            sb.append(String.format("%02x ", buf[i] & 0xff));
        }
        out.println(sb.toString());
    }
}
