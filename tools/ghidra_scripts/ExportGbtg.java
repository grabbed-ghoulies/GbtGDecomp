// Ghidra post-script: dump AID strings and a few oracle xrefs from Grabbed by the Ghoulies.
// Copy to your Ghidra scripts directory or pass -postScript with this file.
//@category GbtG

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;

import java.util.TreeSet;

public class ExportGbtg extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        TreeSet<String> aids = new TreeSet<String>();
        Address addr = mem.getMinAddress();
        while (addr != null && addr.getOffset() < mem.getMaxAddress().getOffset()) {
            Data d = getDataAt(addr);
            if (d != null && d.hasStringValue()) {
                String s = d.getDefaultValueRepresentation();
                if (s.contains("aid_")) {
                    aids.add(s);
                }
            }
            try {
                addr = addr.add(1);
            } catch (Exception ex) {
                break;
            }
            if (monitor.isCancelled()) {
                break;
            }
        }
        println("AID-like defined strings: " + aids.size());
        int n = 0;
        for (String s : aids) {
            println("  " + s);
            if (++n > 80) {
                println("  ...");
                break;
            }
        }
        String[] needles = {
            "bundles\\", "demand\\", "audioLoadFile",
            "There is no script for this scene!", "ndVertexBuffer"
        };
        for (String needle : needles) {
            println("xrefs to \"" + needle + "\":");
            Address found = find(needle);
            int count = 0;
            while (found != null && count < 8) {
                Reference[] refs = getReferencesTo(found);
                for (Reference r : refs) {
                    println("  " + r.getFromAddress());
                }
                found = find(found.add(1), needle);
                count++;
            }
        }
    }
}
