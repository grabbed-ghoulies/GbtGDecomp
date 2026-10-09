//@category GbtG
import java.io.*;
import java.util.*;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;

// args: outdir translatedFile root1 root2 ... ; vt:ADDR:COUNT adds vtable entries as roots
public class CalleeTree extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        File outDir = new File(a[0]);
        Set<Long> done = new HashSet<>();
        try (BufferedReader r = new BufferedReader(new FileReader(a[1]))) {
            String l; while ((l = r.readLine()) != null) { l = l.trim(); if (!l.isEmpty()) done.add(Long.parseUnsignedLong(l, 16)); }
        }
        Deque<long[]> q = new ArrayDeque<>();
        for (int i = 2; i < a.length; i++) {
            if (a[i].startsWith("vt:")) {
                String[] p = a[i].split(":");
                long base = Long.parseUnsignedLong(p[1], 16); int n = Integer.parseInt(p[2]);
                for (int k = 0; k < n; k++) {
                    long w = Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(base + 4L * k)));
                    println(String.format("VT %08X[%d] = %08X %s", base, k, w, getFunctionAt(toAddr(w)) == null ? "(no fn)" : ""));
                    q.add(new long[]{w, 0, base});
                }
            } else q.add(new long[]{Long.parseUnsignedLong(a[i], 16), 0, 0});
        }
        Set<Long> seen = new HashSet<>();
        DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
        PrintWriter tree = new PrintWriter(new File(outDir, "callee_tree.txt"));
        while (!q.isEmpty()) {
            long[] e = q.poll(); long va = e[0];
            if (!seen.add(va)) continue;
            boolean tr = done.contains(va);
            Function fn = getFunctionAt(toAddr(va));
            int size = fn == null ? -1 : (int) fn.getBody().getNumAddresses();
            tree.printf("%08X parent=%08X size=%d %s%n", va, e[2], size, tr ? "TRANSLATED" : (fn == null ? "NOFUNC" : "TODO"));
            if (tr || fn == null) continue;
            File f = new File(outDir, String.format("%08X.txt", va));
            if (!f.exists()) {
                var res = d.decompileFunction(fn, 120, monitor);
                try (PrintWriter out = new PrintWriter(f)) {
                    out.printf("va=0x%08X%nfn %s%n%n", va, fn.getName());
                    out.println(res.getDecompiledFunction() == null ? ("DECOMPILE FAILED " + res.getErrorMessage()) : res.getDecompiledFunction().getC());
                    out.println("instructions:");
                    for (var ins : currentProgram.getListing().getInstructions(fn.getBody(), true)) out.printf("%s %s%n", ins.getAddress(), ins);
                    out.println("callees:");
                    for (Function c : fn.getCalledFunctions(monitor)) out.printf("0x%s %s%n", c.getEntryPoint(), c.getName());
                }
            }
            for (Function c : fn.getCalledFunctions(monitor)) {
                long cv = c.getEntryPoint().getOffset();
                if (c.isThunk() || c.isExternal()) { tree.printf("  ext %08X %s%n", cv, c.getName()); continue; }
                q.add(new long[]{cv, 0, va});
            }
        }
        tree.close(); d.dispose();
    }
}
