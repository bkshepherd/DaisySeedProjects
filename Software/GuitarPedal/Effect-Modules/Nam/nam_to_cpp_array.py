#!/usr/bin/env python3
"""
nam_to_cpp_array.py  –  Convert a NAM A2 Nano model (.nam JSON) to a C++ float array
                         for inclusion in model_data_nam_a2.h.

Usage:
    python3 nam_to_cpp_array.py <path/to/model.nam> [varname]

  <path/to/model.nam>   Path to the .nam file (SlimmableContainer or WaveNet).
  [varname]             Optional C++ variable name (default: derived from filename).

The script picks the lowest-CPU submodel (max_value closest to 0.5) when the
file uses the SlimmableContainer architecture.

Example:
    python3 nam_to_cpp_array.py "JCM800-MODIFIED.nam" kWeightsJcm800

Output is printed to stdout; redirect it into model_data_nam_a2.h or a scratch file.

After adding the array to model_data_nam_a2.h:
  1. Add a NamA2ModelEntry to kNamA2Models[], using the suggested outputGain
     printed to stderr (for loudness matching across models).
  2. Add the display name to s_modelBinNames[] in nam_a2_module.cpp.
  3. The valueBinCount and kNamA2ModelCount are computed automatically.
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


# Reference loudness (dB) that outputGain values are normalized toward. The
# absolute value is arbitrary — only the spread between models matters for
# loudness matching. Pick the loudness of whichever model you treat as the
# reference (currently JCM800) and keep it consistent across exports.
TARGET_LOUDNESS_DB = -18.0


def extract_loudness(path: str):
    """Return the model's measured loudness in dB, or None if not recorded.

    NAM stores this under top-level metadata; raw weight exports drop it, which
    is why per-model levels drift. SlimmableContainer files keep metadata at the
    top level too."""
    with open(path) as f:
        d = json.load(f)
    meta = d.get("metadata") or {}
    loudness = meta.get("loudness", d.get("loudness"))
    return float(loudness) if isinstance(loudness, (int, float)) else None


def suggest_gain(loudness_db) -> str:
    """Suggested NamA2ModelEntry.outputGain string for this model."""
    if loudness_db is None:
        return "1.0f  // no loudness metadata in .nam; tune by ear"
    gain = 10.0 ** ((TARGET_LOUDNESS_DB - loudness_db) / 20.0)
    return f"{gain:.4f}f  // loudness {loudness_db:.2f} dB -> target {TARGET_LOUDNESS_DB:.1f} dB"


def make_varname(filepath: str) -> str:
    base = os.path.splitext(os.path.basename(filepath))[0]
    # Strip leading [AMP] / [CAB] / [PRE] tags
    base = re.sub(r"^\[.*?\]\s*", "", base)
    # Replace runs of non-alphanumeric chars with underscores
    base = re.sub(r"[^A-Za-z0-9]+", "_", base).strip("_")
    # Camel-case: "JCM_800" -> "kWeightsJcm800"
    parts = base.split("_")
    camel = "".join(p.capitalize() for p in parts if p)
    return "kWeights" + camel


def to_cpp_array(varname: str, weights: list, source_file: str) -> str:
    lines = [
        "// ---------------------------------------------------------------------------",
        f"// Source: {os.path.basename(source_file)}  ({len(weights)} weights)",
        "// ---------------------------------------------------------------------------",
        "NAM_A2_MODEL_DATA NAM_A2_ALIGN32",
        f"inline constexpr float {varname}[nam_a2_daisy::kA2WeightCount] = {{",
    ]
    row: list[str] = []
    for i, w in enumerate(weights):
        row.append(repr(float(w)) + "f")
        if len(row) == 8 or i == len(weights) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    varname = sys.argv[2] if len(sys.argv) >= 3 else make_varname(path)

    weights = extract_weights(path)
    loudness = extract_loudness(path)
    print(
        f"// Extracted {len(weights)} weights from: {os.path.basename(path)}",
        file=sys.stderr,
    )
    print(
        f"// Suggested NamA2ModelEntry.outputGain: {suggest_gain(loudness)}",
        file=sys.stderr,
    )
    print(to_cpp_array(varname, weights, path))


if __name__ == "__main__":
    main()
