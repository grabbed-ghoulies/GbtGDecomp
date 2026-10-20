"""Compare two screenshot sets pixel by pixel (GBTG_OPTIMIZE parity).
Run with an installed Python and Pillow:
  tools/compare_screenshots.py cache/render_perf/optflag_retail cache/render_perf/optflag_opt
Each argument is a stem prefix; files named <prefix>_<case>_<WxH>.bmp are paired by suffix.
Exit status 1 when any pair differs by more than --tolerance in a channel.
"""
import argparse, pathlib, sys
from PIL import Image, ImageChops

p = argparse.ArgumentParser()
p.add_argument('reference')
p.add_argument('candidate')
p.add_argument('--tolerance', type=int, default=1)
p.add_argument('--diff-dir', default='cache/render_perf/diff')
a = p.parse_args()

reference = pathlib.Path(a.reference)
candidate = pathlib.Path(a.candidate)
pairs = []
for file in sorted(reference.parent.glob(reference.name + '_*.bmp')):
    suffix = file.name[len(reference.name):]
    other = candidate.parent / (candidate.name + suffix)
    pairs.append((suffix, file, other))
if not pairs:
    sys.exit(f'no screenshots match {reference}_*.bmp')

failed = False
diff_dir = pathlib.Path(a.diff_dir)
for suffix, left_path, right_path in pairs:
    if not right_path.exists():
        print(f'{suffix}: MISSING {right_path}')
        failed = True
        continue
    left = Image.open(left_path).convert('RGB')
    right = Image.open(right_path).convert('RGB')
    if left.size != right.size:
        print(f'{suffix}: SIZE {left.size} vs {right.size}')
        failed = True
        continue
    diff = ImageChops.difference(left, right)
    maximum = max(high for _, high in diff.getextrema())
    histogram = diff.convert('L').histogram()
    differing = sum(histogram[a.tolerance + 1:])
    status = 'OK' if maximum <= a.tolerance else 'DIFF'
    print(f'{suffix}: {status} max_channel_delta={maximum} pixels_over_tolerance={differing}')
    if maximum > a.tolerance:
        failed = True
        diff_dir.mkdir(parents=True, exist_ok=True)
        diff.point(lambda value: min(255, value * 16)).save(diff_dir / ('diff' + suffix.replace('.bmp', '.png')))
sys.exit(1 if failed else 0)
