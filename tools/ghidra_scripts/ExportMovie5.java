//@category GbtG
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ExportMovie5 extends GhidraScript {
    @Override
    public void run() throws Exception {
        File root = new File(sourceFile.getAbsolutePath()).getParentFile();
        for (int i = 0; i < 8 && root != null; i++) {
            if (new File(root, "CMakeLists.txt").isFile()) {
                break;
            }
            root = root.getParentFile();
        }
        PrintWriter out = new PrintWriter(new FileWriter(new File(root, "cache/ghidra_movie5.txt")), true);
        Memory mem = currentProgram.getMemory();
        String[] needles = {
            "aid_movie_ghoulies_attractmode_logoscaleup",
            "aid_movie_ghoulies_gameselect_house",
            "binklogo"
        };
        for (String needle : needles) {
            out.println("== " + needle + " ==");
            byte[] raw = needle.getBytes(StandardCharsets.US_ASCII);
            Address from = currentProgram.getMinAddress();
            int hits = 0;
            while (from != null && hits < 8) {
                Address hit = mem.findBytes(from, raw, null, true, monitor);
                if (hit == null) {
                    break;
                }
                out.println("  str " + hit);
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(hit);
                while (refs.hasNext()) {
                    Reference r = refs.next();
                    Function fn = getFunctionContaining(r.getFromAddress());
                    out.println("    xref " + r.getFromAddress() + (fn == null ? "" : " " + fn.getName() + " " + fn.getEntryPoint()));
                }
                from = hit.add(1);
                hits++;
            }
        }
        Function fn = getFunctionContaining(toAddr(0xDD774L));
        out.println("== contains dd774 " + (fn == null ? "none" : fn.getName() + " " + fn.getEntryPoint() + ".." + fn.getBody().getMaxAddress()) + " ==");
        out.close();
        println("wrote cache/ghidra_movie5.txt");
    }
}
