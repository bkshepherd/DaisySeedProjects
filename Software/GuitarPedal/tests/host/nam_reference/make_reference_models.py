#!/usr/bin/env python3
"""Writes one reference .nam file per model of the NAM effect: the A2 Lite network of a_template (a2_lite_template.json) with the weights of
Effect-Modules/Nam/models/<name>.h. Usage: make_reference_models.py <outdir>. Also writes <outdir>/order.txt (the model names and output gains in firmware order)."""
import glob, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..", "..")
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
tpl = json.load(open(os.path.join(HERE, "a2_lite_template.json")))
order = []
gains = []
for f in sorted(glob.glob(os.path.join(ROOT, "Effect-Modules", "Nam", "models", "*.h"))):
    t = open(f).read()
    w = [float(x) for x in re.findall(r"(-?[0-9][0-9.e+-]*)f\b", t[t.index("kWeights"):].split("= {", 1)[1].split("};", 1)[0])]
    stem = os.path.splitext(os.path.basename(f))[0]
    d = dict(tpl)
    d["weights"] = w
    d["metadata"] = {"name": stem}
    json.dump(d, open(os.path.join(out, stem + ".nam"), "w"))
    order.append(stem)
    gains.append(re.search(r"kOutputGain = ([0-9.e+-]+)f", t).group(1))
    print(f"{stem}: {len(w)} weights")
open(os.path.join(out, "order.txt"), "w").write("\n".join(f"{n} {g}" for n, g in zip(order, gains)) + "\n")
