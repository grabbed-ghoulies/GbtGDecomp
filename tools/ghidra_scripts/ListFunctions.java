// List every function in the program: address, size in bytes, memory block, thunk flag and name.
// Read-only; writes cache/function_list.txt for the completion census (tools/census/completion.py).
//@category GbtG

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.MemoryBlock;

public class ListFunctions extends ExportEngine {

    @Override
    public void run() throws Exception {
        java.io.File out = cacheFile("function_list.txt");
        int count = 0;
        try (PrintWriter writer = new PrintWriter(out, StandardCharsets.UTF_8.name())) {
            writer.println("# va size block thunk name");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                MemoryBlock block = currentProgram.getMemory().getBlock(function.getEntryPoint());
                writer.printf("%08x %d %s %d %s%n", function.getEntryPoint().getOffset(),
                    function.getBody().getNumAddresses(), block == null ? "-" : block.getName(),
                    function.isThunk() ? 1 : 0, function.getName());
                ++count;
            }
        }
        println("ListFunctions: " + count + " functions to " + out.getAbsolutePath());
    }
}
