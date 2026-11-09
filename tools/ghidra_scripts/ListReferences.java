// List every reference into game code: calls, tail jumps to a function entry, and data references (vtables,
// handler tables, pushed callbacks). Read-only; writes cache/reference_list.txt for the reachability census
// (tools/census/reachability.py).
//
// Each row is `from to type from_function from_block`: from_function is the entry of the function holding the
// reference (`-` when Ghidra has none there), from_block the memory block it lies in.
//@category GbtG

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

public class ListReferences extends ExportEngine {

    private static final long GAME_BEGIN = 0x00011000L;
    private static final long GAME_END = 0x0014A840L;

    @Override
    public void run() throws Exception {
        java.io.File out = cacheFile("reference_list.txt");
        ReferenceManager references = currentProgram.getReferenceManager();
        int count = 0;
        try (PrintWriter writer = new PrintWriter(out, StandardCharsets.UTF_8.name())) {
            writer.println("# from to type from_function from_block");
            AddressIterator sources = references.getReferenceSourceIterator(currentProgram.getMinAddress(), true);
            while (sources.hasNext() && !monitor.isCancelled()) {
                Address from = sources.next();
                for (Reference reference : references.getReferencesFrom(from)) {
                    Address to = reference.getToAddress();
                    if (!to.isMemoryAddress()) {
                        continue;
                    }
                    long target = to.getOffset();
                    if (target < GAME_BEGIN || target >= GAME_END) {
                        continue;
                    }
                    RefType type = reference.getReferenceType();
                    Function targetFunction = currentProgram.getFunctionManager().getFunctionAt(to);
                    String kind;
                    if (type.isCall()) {
                        kind = "call";
                    } else if (type.isData()) {
                        kind = "data";
                    } else if (type.isJump() && targetFunction != null) {
                        kind = "jump";
                    } else {
                        continue;
                    }
                    Function source = currentProgram.getFunctionManager().getFunctionContaining(from);
                    MemoryBlock block = currentProgram.getMemory().getBlock(from);
                    writer.printf("%08x %08x %s %s %s%n", from.getOffset(), target, kind,
                        source == null ? "-" : String.format("%08x", source.getEntryPoint().getOffset()),
                        block == null ? "-" : block.getName());
                    ++count;
                }
            }
        }
        println("ListReferences: " + count + " references to " + out.getAbsolutePath());
    }
}
