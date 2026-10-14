"""Release native-path benchmark. Actor updates remain held; legacy owners are labelled.
Run with an installed Python: tools/render_benchmark.py --label baseline
"""
import argparse, csv, json, math, pathlib, subprocess, time
p = argparse.ArgumentParser()
p.add_argument('--exe', default='build/Release/gbtg.exe')
p.add_argument('--label', required=True)
p.add_argument('--timeout', type=int, default=900)
p.add_argument('--reference', action='store_true')
p.add_argument('--cpu-vertex', action='store_true')
p.add_argument('--cases', nargs='*')
p.add_argument('--resolutions', nargs='*')
p.add_argument('--resume', action='store_true')
p.add_argument('--replay-after', type=int, default=0)
a = p.parse_args()
out = pathlib.Path('cache/render_perf')
out.mkdir(parents=True, exist_ok=True)
cases = [('hall', 'chapter1_scene1_2playcam', 300, 260),
         ('tutorial', 'chapter1_scene1_2playcam', 720, 600),
         ('attract', 'attractsequence_final1', 300, 120),
         ('menu', 'gameselect_gameselect', 300, 120),
         ('cutscene', 'scene0_1book', 300, 120),
         ('transition', 'chapter1_scene1_2playcam', 180, 0)]
results_file = out / f'{a.label}_results.json'
results = json.loads(results_file.read_text()) if a.resume and results_file.exists() else []
def quantile(values, q):
    values = sorted(values)
    return values[min(len(values)-1, math.ceil(q*len(values))-1)]
for width, height in [(640,480),(1280,720),(1920,1080)]:
    if a.resolutions and f'{width}x{height}' not in a.resolutions:
        continue
    for name, scene, frames, warm in cases:
        if a.cases and name not in a.cases:
            continue
        if any(r['case'] == name and r['resolution'] == [width,height] and r.get('exit_code') == 0 for r in results):
            continue
        stem = out / f'{a.label}_{name}_{width}x{height}'
        cmd = [a.exe, '--headless', '--frames', str(frames), '--scene', scene,
               '--width', str(width), '--height', str(height), '--vsync', '0',
               '--render-profile', str(stem)+'.csv', '--screenshot', str(stem)+'.bmp']
        if a.replay_after:
            cmd += ['--render-replay-after', str(a.replay_after)]
        if a.reference:
            cmd.append('--render-reference')
        if a.cpu_vertex:
            cmd.append('--cpu-vertex')
        result = dict(case=name, resolution=[width,height], command=cmd,
                      actor_update_hold=True, replay=bool(a.replay_after), build='Release',
                      nonsteady=name == 'transition', owner='native scene' if name in ('hall','tutorial','transition') else 'existing frontend/cutscene owner')
        with open(str(stem)+'.log', 'w') as log:
            try:
                run = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout)
                result['exit_code'] = run.returncode
            except subprocess.TimeoutExpired:
                result['error'] = 'timeout; incomplete run, no performance acceptance'
        result['log'] = str(stem)+'.log'
        result['screenshot'] = str(stem)+'.bmp'
        result['metadata'] = [line.strip() for line in pathlib.Path(result['log']).read_text(errors='replace').splitlines() if line.startswith('profile ') or line.startswith('GbtGDecomp') or line.startswith('frames ')]
        if result.get('exit_code') == 0:
            with open(str(stem)+'.csv') as f:
                all_rows = list(csv.DictReader(f))
                rows = [r for r in all_rows if int(r['frame']) >= warm]
            result['warm_up_frames'] = warm
            result['cold_max_frame_ms'] = max(float(r['frame_ms']) for r in all_rows[:warm]) if warm else None
            for r in rows:
                r['renderer_cpu_ms'] = float(r['presenter_ms']) + float(r.get('recording_ms',0)) + float(r.get('host_rendering_ms',0))
                r['game_update_residual_ms'] = max(0, float(r['tick_ms']) - r['renderer_cpu_ms'])
            result['sample_count'] = len(rows)
            result['metrics'] = {k: {tag:quantile([float(r[k]) for r in rows],q)
                for tag,q in [('median',.5),('p95',.95),('p99',.99)]}
                for k in rows[0] if k != 'frame'} if rows else {}
            with open(str(stem)+'.csv.gpu.csv') as f:
                gpu = [float(r['gpu_ms']) for r in csv.DictReader(f) if int(r['frame']) >= warm]
            with open(str(stem)+'.csv.gpu.csv') as f:
                busy = [float(r['gpu_draw_ms']) for r in csv.DictReader(f) if int(r['frame']) >= warm and r.get('gpu_draw_ms')]
            result['gpu_draw_ms'] = {tag:quantile(busy,q) for tag,q in [('median',.5),('p95',.95),('p99',.99)]} if busy else None
            result['gpu_draw_samples'] = len(busy)
            result['gpu_samples'] = len(gpu)
            result['gpu_ms'] = {tag:quantile(gpu,q) for tag,q in [('median',.5),('p95',.95),('p99',.99)]} if gpu else None
        results.append(result)
        results_file.write_text(json.dumps(results,indent=2))
        print(name,width,height,result.get('exit_code',result.get('error')),flush=True)
