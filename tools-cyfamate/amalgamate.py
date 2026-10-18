import os, re, sys
ROOT = sys.argv[1]; OUT = sys.argv[2]
SOURCES = """main.cpp types.cpp bitboard.cpp misc.cpp movegen.cpp position.cpp usioption.cpp thread.cpp tt.cpp
movepick.cpp timeman.cpp memory.cpp engine.cpp search.cpp score.cpp benchmark.cpp tune.cpp book/book.cpp book/apery_book.cpp
book/policybook.cpp book/makebook.cpp book/makebook2025.cpp extra/bitop.cpp extra/long_effect.cpp extra/sfen_packer.cpp mate/mate.cpp mate/mate1ply_without_effect.cpp
mate/mate1ply_with_effect.cpp mate/mate_solver.cpp eval/evaluate_bona_piece.cpp eval/evaluate.cpp eval/evaluate_io.cpp
eval/evaluate_mir_inv_tools.cpp eval/material/evaluate_material.cpp testcmd/unit_test.cpp testcmd/mate_test_cmd.cpp
testcmd/normal_test_cmd.cpp eval/nnue/evaluate_nnue.cpp eval/nnue/nnue_test_command.cpp eval/nnue/features/k.cpp
eval/nnue/features/p.cpp eval/nnue/features/a2.cpp eval/nnue/features/half_kp.cpp eval/nnue/features/half_ka1.cpp
eval/nnue/features/half_ka_hm1.cpp eval/nnue/features/half_ka2.cpp eval/nnue/features/half_ka_hm2.cpp
eval/nnue/features/half_kp_vm.cpp eval/nnue/features/half_relative_kp.cpp eval/nnue/features/half_kpe9.cpp
eval/nnue/features/pe9.cpp engine/yaneuraou-engine/yaneuraou-search.cpp usi.cpp""".split()
PRELUDE = """#if !defined(CYFAMATE_TESTING)
#define NDEBUG
#endif
#include <iterator>
#if !defined(CYFAMATE_DISABLE_FINNY_TABLES)
#define USE_FINNY_TABLES
#endif
#define _LINUX
#define UNICODE
#define NO_EXCEPTIONS
#define IS_64BIT
#define YANEURAOU_ENGINE_NNUE
#define SFNNwoPSQT
#define NNUE_SFNN_HIDDEN1_7
#define FOR_TOURNAMENT
#define NNUE_ARCHITECTURE_HEADER "architectures/SFNN_halfka2_1024_7_64_k3k3.h"
#define TARGET_CPU "AVX512VNNI"
#define USE_AVX512
#define USE_VNNI
#define USE_BMI2"""
INC = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
IF = re.compile(r'^\s*#\s*if')
ENDIF = re.compile(r'^\s*#\s*endif')
ONCE = re.compile(r'^\s*#\s*pragma\s+once')
done = set()
out = []
ids = []
cur = [-1]

def resolve(name, curdir):
    for base in (curdir, ROOT, os.path.join(ROOT, 'eval', 'nnue')):
        p = os.path.normpath(os.path.join(base, name))
        if os.path.isfile(p):
            return p
    return None

def rel(p):
    return os.path.relpath(p, ROOT).replace(os.sep, '/')

def expand(path, stack, in_cond):
    txt = open(path, encoding='utf-8-sig', errors='replace').read().replace('\r\n', '\n')
    if rel(path) == 'eval/nnue/nnue_architecture.h':
        txt = re.sub(r'^\s*#\s*include\s+"architectures/[^"]+"\s*$', '', txt, flags=re.M)
        txt = txt.replace('#include NNUE_ARCHITECTURE_HEADER', '#include "architectures/SFNN_halfka2_1024_7_64_k3k3.h"')
    lines = txt.split('\n')
    guard = 0
    for i, l in enumerate(lines):
        s = l.strip()
        if not s or s.startswith('//') or ONCE.match(l):
            continue
        m = re.match(r'\s*#\s*ifndef\s+(\w+)', l)
        if m and i + 1 < len(lines) and re.match(r'\s*#\s*define\s+' + m.group(1) + r'\b', lines[i + 1]):
            guard = 1
        break
    depth = 0
    for line in lines:
        if ONCE.match(line):
            continue
        if IF.match(line):
            depth += 1
        elif ENDIF.match(line):
            depth -= 1
        m = INC.match(line)
        if not m:
            out.append(line)
            continue
        p = resolve(m.group(1), os.path.dirname(path))
        if p is None:
            out.append(line)
            continue
        if p in stack or p in done:
            continue
        cond = in_cond or (depth - guard) > 0
        if not cond:
            done.add(p)
        k = len(ids); ids.append(p)
        parent = cur[-1]
        out.append(('BEGIN', k, parent))
        cur.append(k)
        expand(p, stack + [p], cond)
        cur.pop()
        out.append(('END', k, parent))

out.append(open(os.path.join(ROOT, 'cyfamate', 'prelude.inc'), encoding='utf-8').read().rstrip(chr(10)))
for s_ in SOURCES:
    p = os.path.join(ROOT, s_)
    out.append('#undef Options')
    out.append('#undef Threads')
    out.append('// [amalg] ===== TU ' + s_ + ' =====')
    expand(p, [p], False)

def render(keep, markers):
    res = []; skip = 0
    for e in out:
        if isinstance(e, tuple):
            kind, k, parent = e
            if kind == 'BEGIN':
                if skip or k not in keep:
                    skip += 1; continue
                res.append('// [amalg] >>> ' + rel(ids[k]))
                if markers: res.append('#line 1 "@@AMALG_%d"' % k)
            else:
                if skip:
                    if k not in keep or skip > 1: skip -= 1; continue
                    skip -= 1; continue
                res.append('// [amalg] <<< ' + rel(ids[k]))
                if markers: res.append('#line 1 "@@AMALG_%d"' % parent)
        elif not skip:
            res.append(e)
    return '\n'.join(res) + '\n'

import subprocess
allk = set(range(len(ids)))
if len(sys.argv) > 3:
    open(OUT + '.probe.cpp', 'w', encoding='utf-8', newline='\n').write(render(allk, True))
    pre = subprocess.run(sys.argv[3].split() + [OUT + '.probe.cpp'], capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
    live = set(int(x) for x in re.findall(r'"@@AMALG_(\d+)"', pre))
    keep = set()
    first = {}
    for k, p in enumerate(ids):
        first.setdefault(p, k)
    has_live = set(ids[k] for k in live)
    for k, p in enumerate(ids):
        if k in live or (p not in has_live and first[p] == k):
            keep.add(k)
    print('expansions', len(ids), 'live', len(live), 'keep', len(keep))
else:
    keep = allk
txt = render(keep, False)
open(OUT, 'w', encoding='utf-8', newline='\n').write(txt)
print(txt.count('\n'), 'lines')
