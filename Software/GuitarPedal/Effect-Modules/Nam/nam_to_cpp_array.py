#!/usr/bin/env python3
"""
nam_to_cpp_array.py  -  Convert a NAM A2 Nano model (.nam JSON) to a model file for the NAM effect
                         (one header per model in Effect-Modules/Nam/models/).

Usage:
    python3 nam_to_cpp_array.py <path/to/model.nam> "<name to show>" <models/name.h> [output gain]

  <path/to/model.nam>   Path to the .nam file (SlimmableContainer or WaveNet).
  <name to show>        The name of the model in the MODEL parameter's choices (keep it short: the display is small).
  <models/name.h>       The model file to write. The models are listed in file-name order (alphabetical)
                        in the MODEL choices; use only letters, digits and _ in the name.
  [output gain]         Linear output-gain multiplier for loudness matching (default 1.0; calibrate it afterwards
                        with tests/host/run_level_tests.sh).

The script picks the lowest-CPU submodel (max_value closest to 0.5) when the file uses the SlimmableContainer architecture.

Example:
    python3 nam_to_cpp_array.py "JCM800-MODIFIED.nam" "JCM800" models/jcm800_modified.h

The Makefile runs update_models.sh, which picks up the new file (run it by hand if you build another way);
nothing else needs to be edited.
"""

import json
import os
import re
import sys


def extract_weights(path: str) -> list:
    with open(path) as f:
        d = json.load(f)
    arch = d.get("architecture", "")
    if arch == "SlimmableContainer":
        submodels = d["config"]["submodels"]
        sub = min(submodels, key=lambda s: abs(s["max_value"] - 0.5))
        model = sub["model"]
    elif arch == "WaveNet":
        model = d
    else:
        raise ValueError(f"Unsupported architecture: {arch!r}")
    weights = model.get("weights")
    if not isinstance(weights, list):
        raise ValueError("No flat 'weights' list found in model.")
    return weights


def to_model_file(name: str, gain: float, weights: list, source_file: str, stem: str) -> str:
    lines = [
        "#pragma once",
        f'// NAM A2 model "{name}", source: {os.path.basename(source_file)}',
        f"// ({len(weights)} weights, A2 Nano/Lite: C=3 channels, 23 layers).",
        "// Picked up automatically: models/*.h are listed in file-name order by update_models.sh.
        "",
        '#include "../nam_a2_runtime.h" // NAM_A2_MODEL_DATA, NAM_A2_ALIGN32, kA2WeightCount',
        "",
        f"namespace nam_model_{stem}",
        "{",
        "",
        "// The name shown for the MODEL parameter.",
        f'inline constexpr const char *kName = "{name}";',
        "",
        "// Linear output-gain multiplier for loudness matching (1.0 = unity; below 1.0 makes a hot model quieter).",
        "// Calibrated with tests/host/run_level_tests.sh so every model gives about +9 dB RMS at default settings.",
        f"inline constexpr float kOutputGain = {gain!r}f;",
        "",
        "NAM_A2_MODEL_DATA NAM_A2_ALIGN32",
        "inline constexpr float kWeights[nam_a2_daisy::kA2WeightCount] = {",
    ]
    row: list[str] = []
    for i, w in enumerate(weights):
        row.append(repr(float(w)) + "f")
        if len(row) == 8 or i == len(weights) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines += ["};", "", f"}} // namespace nam_model_{stem}", ""]
    return "\n".join(lines)


def main() -> None:
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    path, name, out = sys.argv[1], sys.argv[2], sys.argv[3]
    gain = float(sys.argv[4]) if len(sys.argv) >= 5 else 1.0
    stem = os.path.splitext(os.path.basename(out))[0]
    if not re.fullmatch(r"[A-Za-z0-9_]+", stem):
        sys.exit(f"{out}: use only letters, digits and _ in the file name")

    weights = extract_weights(path)
    with open(out, "w") as f:
        f.write(to_model_file(name, gain, weights, path, stem))
    print(f"wrote {out}: {len(weights)} weights from {os.path.basename(path)}; calibrate kOutputGain (now {gain}) with tests/host/run_level_tests.sh")


if __name__ == "__main__":
    main()
