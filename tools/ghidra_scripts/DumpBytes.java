//@category GbtG
import java.io.*;
import ghidra.app.script.GhidraScript;
public class DumpBytes extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        for (int i = 1; i < a.length; i++) {
            String[] r = a[i].split("-");
            long s = Long.parseUnsignedLong(r[0], 16), e = Long.parseUnsignedLong(r[1], 16);
            byte[] b = new byte[(int)(e - s)];
            int n = currentProgram.getMemory().getBytes(toAddr(s), b);
            try (FileOutputStream o = new FileOutputStream(new File(a[0], String.format("mem_%08X.bin", s)))) { o.write(b, 0, n); }
            println("dumped " + n);
        }
    }
}
