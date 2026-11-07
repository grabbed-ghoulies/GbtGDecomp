"""Generate src/engine/plugins/object_schemas_generated.inc: the named retail structs (docs/RETAIL_LAYOUTS.md) as
field lists the debug UI's object inspector walks.

A struct is listed when its header declares it at namespace scope with `static_assert(sizeof(X) == N)` beside it,
which is how a consolidated retail layout is marked. Only member names and comments are read here; the compiler
works out each member's offset, size and kind (src/engine/plugins/object_schemas.h), so a renamed or removed member
breaks the build until this is run again:

    python tools/layout/object_schemas.py
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SRC = os.path.join(ROOT, 'src')
OUT = os.path.join(SRC, 'engine', 'plugins', 'object_schemas_generated.inc')

# Inspector-only and frontend headers that need a configuration of their own; their structs are not factory objects.
SKIP_DIRS = {os.path.join('engine', 'inspector'), 'plugins', 'tools'}

DECL_SKIP = re.compile(r'^\s*(static|using|friend|typedef|template|struct|class|enum|union|public|private|protected)\b')


def strip_comments(line):
    """The code part of a line and its trailing // comment."""
    at = line.find('//')
    if at < 0:
        return line, ''
    return line[:at], line[at + 2:].strip()


def namespace_depth_ok(text, pos):
    """True when `pos` is at namespace scope: every open brace before it belongs to a namespace."""
    depth_kinds = []
    i = 0
    token = re.compile(r'namespace\s*\w*\s*\{|\{|\}|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', re.S)
    for m in token.finditer(text, 0, pos):
        s = m.group(0)
        if s.startswith('//') or s.startswith('/*') or s.startswith('"'):
            continue
        if s == '}':
            if depth_kinds:
                depth_kinds.pop()
        elif s == '{':
            depth_kinds.append('block')
        else:
            depth_kinds.append('namespace')
    return all(k == 'namespace' for k in depth_kinds)


def struct_body(text, open_brace):
    depth = 0
    for i in range(open_brace, len(text)):
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[open_brace + 1:i]
    return None


def split_top(s, sep):
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '<({[':
            depth += 1
        elif c in '>)}]':
            depth -= 1
        if c == sep and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
    out.append(cur)
    return out


def members(body):
    """(name, note) for each data member at the struct's own depth."""
    result = []
    pending_note = []
    statement = ''
    trailing = ''
    depth = 0
    for raw in body.split('\n'):
        code, comment = strip_comments(raw)
        if depth == 0 and statement == '' and code.strip() == '' and comment:
            pending_note.append(comment)
            continue
        if depth == 0 and statement == '' and code.strip() == '':
            pending_note = []
            continue
        for c in code:
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
        statement += ' ' + code.strip()
        if comment:
            trailing = comment
        if depth == 0 and statement.rstrip().endswith(';'):
            decl = statement.strip()[:-1].strip()
            note = ' '.join(pending_note + ([trailing] if trailing else []))
            statement, trailing, pending_note = '', '', []
            if DECL_SKIP.match(decl):
                continue
            # Strip a top-level initializer, then reject function declarations.
            parts = split_top(decl, ',')
            first = parts[0]
            if '(' in re.sub(r'(=|\{).*$', '', first):
                continue
            type_and_first = re.sub(r'\s*(=.*|\{.*\})$', '', first).strip()
            m = re.match(r'^(.*?[\s*&>])(\w+)\s*(\[[^\]]*\])?$', type_and_first)
            if not m:
                continue
            names = [m.group(2)]
            for extra in parts[1:]:
                extra = re.sub(r'\s*(=.*|\{.*\})$', '', extra).strip()
                em = re.match(r'^[*&\s]*(\w+)\s*(\[[^\]]*\])?$', extra)
                if em:
                    names.append(em.group(1))
            if '&' in m.group(1) or 'static' in m.group(1):
                continue
            for name in names:
                result.append((name, note))
    return result


def c_string(s):
    s = s.replace('\\', '\\\\').replace('"', '\\"')
    return '"' + s + '"'


def main():
    structs = []
    for d, dirs, files in os.walk(SRC):
        rel = os.path.relpath(d, SRC)
        if any(rel == s or rel.startswith(s + os.sep) for s in SKIP_DIRS):
            dirs[:] = []
            continue
        for f in sorted(files):
            if not f.endswith('.h'):
                continue
            path = os.path.join(d, f)
            text = open(path, encoding='utf-8', errors='replace').read()
            if '#if defined(GBTG_INSPECTOR_APP)' in text[:200]:
                continue
            for m in re.finditer(r'static_assert\(sizeof\((\w+)\) == (0x[0-9A-Fa-f]+|\d+)\);', text):
                name = m.group(1)
                sm = re.search(r'\nstruct ' + name + r'\s*\n\{', text)
                if not sm or sm.start() > m.start() or not namespace_depth_ok(text, sm.start() + 1):
                    continue
                body = struct_body(text, sm.end() - 1)
                if body is None:
                    continue
                fields = members(body)
                if fields:
                    include = os.path.relpath(path, os.path.join(SRC, 'engine', 'plugins')).replace(os.sep, '/')
                    structs.append((name, include, fields))
    structs.sort(key=lambda s: s[0])
    names = [s[0] for s in structs]
    if len(set(names)) != len(names):
        dupes = sorted({n for n in names if names.count(n) > 1})
        sys.exit('duplicate struct names: ' + ', '.join(dupes))
    lines = ['// Generated by tools/layout/object_schemas.py from the named retail structs. Do not edit; run the script.',
             '#if defined(GBTG_OBJECT_SCHEMA_INCLUDES)']
    for include in sorted({s[1] for s in structs}):
        lines.append('#include "%s"' % include)
    lines.append('#else')
    for name, _, fields in structs:
        lines.append('GBTG_SCHEMA_BEGIN(%s)' % name)
        for field, note in fields:
            lines.append('    GBTG_SCHEMA_FIELD(%s, %s, %s)' % (name, field, c_string(note)))
        lines.append('GBTG_SCHEMA_END(%s)' % name)
    lines.append('#endif')
    with open(OUT, 'w', encoding='utf-8', newline='\n') as out:
        out.write('\n'.join(lines) + '\n')
    print('%d structs, %d fields -> %s' % (len(structs), sum(len(s[2]) for s in structs), os.path.relpath(OUT, ROOT)))


if __name__ == '__main__':
    main()
