//@category GbtG
import java.io.PrintWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;

// Read-only exports for the first playable scene's dialogue and actor loading.
public class ExportPlayerScene extends GbtgScript {
    @Override
    public void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        try {
            for (String arg : getScriptArgs()) {
                if (arg.startsWith("range-function:")) {
                    String[] range = arg.substring(15).split("-");
                    long first = Long.parseUnsignedLong(range[0], 16);
                    long last = Long.parseUnsignedLong(range[1], 16);
                    int transaction = currentProgram.startTransaction("Temporary export function");
                    try {
                        new ghidra.app.cmd.disassemble.DisassembleCommand(toAddr(first),
                            new ghidra.program.model.address.AddressSet(toAddr(first), toAddr(last)), false)
                            .applyTo(currentProgram, monitor);
                        Function temporary = currentProgram.getFunctionManager().createFunction(null,
                            toAddr(first), new ghidra.program.model.address.AddressSet(toAddr(first), toAddr(last)),
                            ghidra.program.model.symbol.SourceType.DEFAULT);
                        var result = decomp.decompileFunction(temporary, 120, monitor);
                        if (result.getDecompiledFunction() == null) {
                            throw new IllegalStateException(result.getErrorMessage());
                        }
                        try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/ghidra_frontier/%08X.txt", first)))) {
                            out.printf("va=0x%08X%nfn %s (temporary, project unchanged)%n%n", first, temporary.getName());
                            out.println(result.getDecompiledFunction().getC());
                            out.println("instructions:");
                            for (var instruction : currentProgram.getListing().getInstructions(temporary.getBody(), true)) {
                                out.printf("%s %s%n", instruction.getAddress(), instruction);
                            }
                        }
                    } finally {
                        currentProgram.endTransaction(transaction, false);
                        decomp.flushCache();
                    }
                    continue;
                }
                if (arg.startsWith("string-table:")) {
                    String[] parts = arg.substring(13).split(":");
                    long target = Long.parseUnsignedLong(parts[0], 16);
                    int count = Integer.parseInt(parts[1]);
                    try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_strings_%08X.txt", target)))) {
                        for (int index = 0; index < count; ++index) {
                            long pointer = Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(target + index * 4)));
                            StringBuilder value = new StringBuilder();
                            for (long at = pointer;; ++at) {
                                int character = Byte.toUnsignedInt(currentProgram.getMemory().getByte(toAddr(at)));
                                if (character == 0) break;
                                value.append((char)character);
                            }
                            out.printf("%08X %s%n", pointer, value);
                        }
                    }
                    continue;
                }
                if (arg.startsWith("instructions:")) {
                    String[] range = arg.substring(13).split("-");
                    long first = Long.parseUnsignedLong(range[0], 16);
                    long last = Long.parseUnsignedLong(range[1], 16);
                    try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_instructions_%08X.txt", first)))) {
                        for (var instruction : currentProgram.getListing().getInstructions(toAddr(first), true)) {
                            if (instruction.getAddress().getOffset() >= last) break;
                            out.printf("%s %s%n", instruction.getAddress(), instruction);
                        }
                    }
                    continue;
                }
                if (arg.startsWith("functions:")) {
                    String[] range = arg.substring(10).split("-");
                    long first = Long.parseUnsignedLong(range[0], 16);
                    long last = Long.parseUnsignedLong(range[1], 16);
                    try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_functions_%08X.txt", first)))) {
                        for (var fn : currentProgram.getFunctionManager().getFunctions(toAddr(first), true)) {
                            if (fn.getEntryPoint().getOffset() >= last) break;
                            out.printf("%s %s%n", fn.getEntryPoint(), fn.getName());
                        }
                    }
                    continue;
                }
                if (arg.startsWith("words:")) {
                    long target = Long.parseUnsignedLong(arg.substring(6), 16);
                    try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_words_%08X.txt", target)))) {
                        for (int index = 0; index < 32; ++index) {
                            long at = target + index * 4;
                            out.printf("%08X %08X%n", at, currentProgram.getMemory().getInt(toAddr(at)));
                        }
                    }
                    continue;
                }
                if (arg.startsWith("refs:")) {
                    long target = Long.parseUnsignedLong(arg.substring(5), 16);
                    try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_refs_%08X.txt", target)))) {
                        for (var ref : getReferencesTo(toAddr(target))) {
                            Function owner = getFunctionContaining(ref.getFromAddress());
                            out.printf("%s %s%n", ref.getFromAddress(), owner == null ? "" : owner.getEntryPoint());
                        }
                    }
                    continue;
                }
                long va = Long.parseUnsignedLong(arg.replace("0x", ""), 16);
                Function fn = getFunctionAt(toAddr(va));
                if (fn == null) {
                    throw new IllegalStateException("No function at " + arg);
                }
                var result = decomp.decompileFunction(fn, 120, monitor);
                if (result.getDecompiledFunction() == null) {
                    throw new IllegalStateException(result.getErrorMessage());
                }
                try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/ghidra_frontier/%08X.txt", va)))) {
                    out.printf("va=0x%08X%nfn %s%n%n", va, fn.getName());
                    out.println(result.getDecompiledFunction().getC());
                    out.println("instructions:");
                    for (var instruction : currentProgram.getListing().getInstructions(fn.getBody(), true)) {
                        out.printf("%s %s%n", instruction.getAddress(), instruction);
                    }
                    out.println("callees:");
                    for (Function callee : fn.getCalledFunctions(monitor)) {
                        out.printf("0x%s %s%n", callee.getEntryPoint(), callee.getName());
                    }
                }
            }
        } finally {
            decomp.dispose();
        }
    }
}
