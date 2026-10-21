//@category GbtG
import java.io.PrintWriter;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

// Read-only byte scan of the executable blocks for stores and pushes of 32-bit immediates, including code the
// project has not disassembled: MOV dword [ESP+d8],imm (C7 44 24 d8 imm), MOV dword [EBP+d8],imm (C7 45 d8 imm),
// MOV dword [ESP+d32],imm (C7 84 24 d32 imm) and PUSH imm (68 imm). Arguments are hex values; the hits go to
// cache/player_immediates_<first value>.txt with the containing function when the project knows one.
public class FindImmediates extends GbtgScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(repoFile(String.format("cache/player_immediates_%s.txt", args[0])))) {
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isExecute() || !block.isInitialized()) {
                    continue;
                }
                byte[] bytes = new byte[(int) block.getSize()];
                block.getBytes(block.getStart(), bytes);
                for (String arg : args) {
                    long value = Long.parseUnsignedLong(arg, 16);
                    for (int at = 0; at + 5 <= bytes.length; ++at) {
                        int width = 0;
                        int prefix = 0;
                        int b0 = bytes[at] & 0xFF;
                        if (b0 == 0x68) {
                            prefix = 1;
                        } else if (b0 == 0xC7 && at + 1 < bytes.length) {
                            int modrm = bytes[at + 1] & 0xFF;
                            if (modrm == 0x44 && at + 2 < bytes.length && (bytes[at + 2] & 0xFF) == 0x24) {
                                prefix = 4;
                            } else if (modrm == 0x45) {
                                prefix = 3;
                            } else if (modrm == 0x05) {
                                prefix = 6;
                            } else if (modrm == 0x84 && at + 2 < bytes.length && (bytes[at + 2] & 0xFF) == 0x24) {
                                prefix = 7;
                            }
                        }
                        width = prefix;
                        if (width == 0 || at + width + 4 > bytes.length) {
                            continue;
                        }
                        long immediate = (bytes[at + width] & 0xFFL) | ((bytes[at + width + 1] & 0xFFL) << 8)
                            | ((bytes[at + width + 2] & 0xFFL) << 16) | ((bytes[at + width + 3] & 0xFFL) << 24);
                        if (immediate != value) {
                            continue;
                        }
                        var address = block.getStart().add(at);
                        Function owner = getFunctionContaining(address);
                        out.printf("%s %s %s%n", arg, address, owner == null ? "" : owner.getEntryPoint());
                    }
                }
            }
        }
    }
}
