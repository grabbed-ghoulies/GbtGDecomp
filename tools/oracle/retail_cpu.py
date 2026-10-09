"""Shared helpers for differential checks against retail machine code.

Executes code bytes exported from the user's own default.xbe (never committed)
with Unicorn. Export them first, read-only:

  analyzeHeadless <ghidra_projects> GbtG -process default.xbe.original -readOnly -noanalysis \
      -scriptPath tools/ghidra/ghidra_scripts -postScript DumpBytes.java cache/oracle 00185000-001A2000
"""
import os, struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CODE_BASE = 0x00185000
CODE_FILE = os.path.join(ROOT, 'cache', 'oracle', 'mem_%08X.bin' % CODE_BASE)
STOP = 0x00F00000
HEAP = 0x10000000
STACK = 0x20000000

def new_emu():
    code = open(CODE_FILE, 'rb').read()
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(0x00180000, 0x30000)  # Code range plus the validator globals near 0x001A4D30.
    u.mem_write(CODE_BASE, code)
    u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
    u.mem_map(HEAP, 0x100000)
    u.mem_map(STACK - 0x10000, 0x20000)
    return u

def w32(u, a, v): u.mem_write(a, struct.pack('<I', v & 0xffffffff))
def r32(u, a): return struct.unpack('<I', u.mem_read(a, 4))[0]

def call(u, fn, ecx, args, esp=STACK):
    """Calls a __thiscall/__fastcall/__stdcall function and returns EAX."""
    w32(u, esp, STOP)
    for i, a in enumerate(args): w32(u, esp + 4 + 4 * i, a)
    u.reg_write(UC_X86_REG_ESP, esp); u.reg_write(UC_X86_REG_ECX, ecx); u.reg_write(UC_X86_REG_EAX, 0xDEADBEEF)
    u.emu_start(fn, STOP)
    return u.reg_read(UC_X86_REG_EAX)

def harness(name):
    return os.path.join(ROOT, 'cache', 'oracle', name)
