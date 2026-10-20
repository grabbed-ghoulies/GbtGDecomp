"""Side-by-side table of two render_benchmark.py labels (for example retail vs GBTG_OPTIMIZE).
Run with an installed Python: tools/compare_render_benchmarks.py optflag_retail optflag_opt
"""
import argparse, json, pathlib

p = argparse.ArgumentParser()
p.add_argument('baseline')
p.add_argument('candidate')
p.add_argument('--metrics', nargs='*', default=['frame_ms', 'presenter_ms', 'submission_ms', 'recording_ms',
                                                 'vertex_ms', 'texture_ms', 'allocations'])
a = p.parse_args()

def load(label):
    path = pathlib.Path('cache/render_perf') / f'{label}_results.json'
    return {(r['case'], tuple(r['resolution'])): r for r in json.loads(path.read_text())}

baseline, candidate = load(a.baseline), load(a.candidate)
print(f'{"case":<12}{"res":<11}{"metric":<15}{"base med":>10}{"cand med":>10}{"base p99":>10}{"cand p99":>10}')
for key in sorted(baseline):
    if key not in candidate or 'metrics' not in baseline[key] or 'metrics' not in candidate[key]:
        print(f'{key[0]:<12}{"x".join(map(str, key[1])):<11}missing or failed run')
        continue
    for metric in a.metrics:
        left = baseline[key]['metrics'].get(metric)
        right = candidate[key]['metrics'].get(metric)
        if left is None or right is None:
            continue
        print(f'{key[0]:<12}{"x".join(map(str, key[1])):<11}{metric:<15}{left["median"]:>10.3f}{right["median"]:>10.3f}'
              f'{left["p99"]:>10.3f}{right["p99"]:>10.3f}')
