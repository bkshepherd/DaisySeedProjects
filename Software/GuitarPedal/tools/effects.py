#!/usr/bin/env python3
"""Effect configuration tool for the guitar pedal firmware.

Selects which effects are built into the firmware, generates the parts of the build that depend on it, and keeps track of how much memory is left.

  effects.json          the manifest: every effect (class, header, sources, what it needs) and the shared components (CloudSeed pool, CMSIS FFT tables, ...)
  effects.config        the active selection (one line per effect: ID=y); presets are in configs/
  chains.json           effect chains (several effects combined into one menu entry, see EffectChain): slots, knob and MIDI mappings, footswitch
  effects-memory.json   what each effect costs in memory (measured from the object files, plus heap figures that are estimates)
  generated/            what `generate` writes: effects.mk (included by the Makefile), effects_includes.inc and effects_instances.inc (included by loaded_effects.h)

Commands:  tui (the default in a terminal) | list | info ID... | lint | observe RESULT | preset NAME | enable ID... | disable ID... | generate | report | measure | calibrate | matrix [ID...]
See docs/PERFORMANCE_AND_MEMORY.md for what the figures mean and where they come from.
"""
import argparse
import itertools
import json
import os
import re
import subprocess
import sys

ROOT = os.environ.get("EFFECTS_TOOL_ROOT") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # (the variable is for the tests)
MANIFEST = os.path.join(ROOT, "effects.json")
MEMORY = os.path.join(ROOT, "effects-memory.json")
CONFIG = os.path.join(ROOT, "effects.config")
CHAINS = os.path.join(ROOT, "chains.json")
PRESETS = os.path.join(ROOT, "configs")
GEN = os.path.join(ROOT, "generated")

POOL_BYTES = {1: 48 * 384 * 384, 2: 48 * 512 * 512, 3: 48 * 640 * 640}  # the CloudSeed pool for 1, 2 and 3 engines (cloudseed_pool.cpp)
REGIONS = ["dtcm", "ram_d2", "sram", "sdram"]
REGION_NAMES = {"dtcm": "DTCM", "ram_d2": "RAM_D2 (static + heap)", "sram": "SRAM (code)", "sdram": "SDRAM"}


def load_json(path):
    with open(path) as f:
        return json.load(f)


def manifest():
    m = load_json(MANIFEST)
    m["by_id"] = {e["id"]: e for e in m["effects"]}
    return m


def read_config(path=None):
    """Returns (set of enabled effect ids, options dict)."""
    path = path or CONFIG
    enabled, options = set(), {}
    if not os.path.exists(path):
        return enabled, options
    for line in open(path):
        line = line.split("#")[0].strip()
        if not line or "=" not in line:
            continue
        key, value = [x.strip() for x in line.split("=", 1)]
        if key.upper() == key and key.startswith("ALLOW_"):
            options[key] = value == "y"
        elif value == "y":
            enabled.add(key)
    return enabled, options


def write_config(enabled, options, m, path=None, header=None):
    path = path or CONFIG
    lines = ["# Effect selection, edited with tools/effects.py (or by hand: ID=y enables an effect). Order and comments do not matter."]
    if header:
        lines += ["# " + h for h in header.split("\n")]
    lines.append("ALLOW_GPL=%s" % ("y" if options.get("ALLOW_GPL") else "n"))
    for e in m["effects"]:
        lines.append("%s=%s" % (e["id"], "y") if e["id"] in enabled else "# %s is not set" % e["id"])
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def selection_problems(enabled, options, m):
    """What stops a selection from being built: unknown effects, conflicts, GPL effects without ALLOW_GPL (a list of messages; empty if it can be built)."""
    problems = []
    unknown = [i for i in enabled if i not in m["by_id"]]
    if unknown:
        return ["unknown effect(s): %s" % ", ".join(sorted(unknown))]
    effects = [e for e in m["effects"] if e["id"] in enabled]
    comps = []
    for e in effects:
        for c in e.get("requires", []):
            if c not in m["components"]:
                return ["manifest: %s requires unknown component %s" % (e["id"], c)]
            if c not in comps:
                comps.append(c)
    for e in effects:
        for c in e.get("conflicts", []):
            if c in enabled:
                problems.append("%s and %s cannot be built together (%s)" % (e["id"], c, e.get("note", "conflicting definitions")))
    gpl = [e["id"] for e in effects if e.get("license") == "gpl"] + [c for c in comps if m["components"][c].get("license") == "gpl"]
    if gpl and not options.get("ALLOW_GPL"):
        problems.append("%s are GPL-3.0-or-later: a firmware binary that contains them must be distributed under GPLv3 (or not at all); ALLOW_GPL=y accepts that" % ", ".join(sorted(set(gpl))))
    return problems


def resolve(enabled, options, m):
    """The enabled effects in firmware order, and the components they need."""
    problems = selection_problems(enabled, options, m)
    if problems:
        sys.exit("effects.config: " + "; ".join(problems))
    effects = [e for e in m["effects"] if e["id"] in enabled]
    comps = []
    for e in effects:
        for c in e.get("requires", []):
            if c not in comps:
                comps.append(c)
    return effects, comps


def write_if_changed(path, text):
    old = open(path).read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w") as f:
            f.write(text)
        return True
    return False


def generate(quiet=False):
    m = manifest()
    enabled, options = read_config()
    chains = read_chains()
    mem = load_json(MEMORY) if os.path.exists(MEMORY) else None
    problems = all_chain_problems(enabled, chains, m, mem)
    if problems:
        sys.exit("chains.json: " + "; ".join(problems))
    built = effective_enabled(enabled, chains)
    effects, comps = resolve(built, options, m)
    standalone = [e for e in effects if e["id"] in enabled]
    os.makedirs(GEN, exist_ok=True)

    mk = ["# Generated by tools/effects.py from effects.config, chains.json and effects.json: do not edit (run `tools/effects.py enable/disable/preset`, or edit effects.config).",
          "# Enabled effects: %s" % (", ".join(e["id"] for e in effects) or "none")]
    sources, c_sources, defines = [], [], []
    for c in comps:
        sources += m["components"][c].get("sources", [])
        c_sources += m["components"][c].get("c_sources", [])
        for d in m["components"][c].get("defines", []):
            if d not in defines:
                defines.append(d)
    for e in effects:
        sources += e["sources"]
    for s in dict.fromkeys(sources):  # once each: an effect can use another effect's sources (the Dist Bench those of the effects it runs)
        mk.append("CPP_SOURCES += %s" % s)
    for s in c_sources:
        mk.append("C_SOURCES += %s" % s)
    for c in comps:  # components that are shared by several effects get a define with the number of effects that use them
        name = m["components"][c].get("count_define")
        if name:
            defines.append("-D%s=%d" % (name, sum(1 for e in effects if c in e.get("requires", []))))
    changed = write_if_changed(os.path.join(GEN, "effects.mk"), "\n".join(mk) + "\n")
    # The defines go in a file of their own, which every object file depends on (see the Makefile): a changed define (the CloudSeed pool for two engines instead of one, the CMSIS tables
    # of Venus) has to recompile what uses it, and make cannot see that a compiler flag changed. The file is only rewritten when the defines are different.
    dm = ["# Generated by tools/effects.py: do not edit. The defines that depend on the selection; every object file is rebuilt when this file changes."]
    dm += ["C_DEFS += %s" % d for d in defines]
    changed |= write_if_changed(os.path.join(GEN, "defines.mk"), "\n".join(dm) + "\n")

    inc = ["// Generated by tools/effects.py from effects.config and effects.json: do not edit."]
    for e in effects:
        inc.append('#include "../Effect-Modules/%s"%s' % (e["header"], (" // " + e["note"]) if e.get("note") else ""))
    changed |= write_if_changed(os.path.join(GEN, "effects_includes.inc"), "\n".join(inc) + "\n")

    inst = ["// Generated by tools/effects.py from effects.config and effects.json: do not edit. The order is the order in effects.json (it decides the menu order and the layout of the saved settings)."]
    for e in standalone:
        inst.append("new %s(),%s" % (e["class"], ("  // " + e["note"]) if e.get("note") else ""))
    changed |= write_if_changed(os.path.join(GEN, "effects_instances.inc"), "\n".join(inst) + "\n")
    # what each entry of the effect list (the effects, then the chains) is estimated to allocate with new, for the check at startup (see guitar_pedal.cpp and Util/heap_guard.h)
    figures = (mem or {}).get("effects", {})
    heap_of = lambda i: figures.get(i, {}).get("heap") or 0
    heap = ["// Generated by tools/effects.py from effects-memory.json: do not edit. The estimated heap (bytes) of each entry of the effect list, in its order."]
    for e in standalone:
        heap.append("%d, // %s" % (heap_of(e["id"]), e["class"]))
    for c in chains:
        heap.append("%d, // chain %s" % (sum(heap_of(sl["effect"]) for sl in c["slots"]), c["name"]))
    changed |= write_if_changed(os.path.join(GEN, "effects_heap.inc"), "\n".join(heap) + "\n")
    ch = ["// Generated by tools/effects.py from chains.json: do not edit. The chains come after the effects in the menu."]
    for c in chains:
        ch.append(chain_code(c, m))
    changed |= write_if_changed(os.path.join(GEN, "effects_chains.inc"), "\n".join(ch) + "\n")
    if not quiet:
        print("%s: %d effects (%s)%s" % ("generated" if changed else "up to date", len(standalone), ", ".join(e["id"] for e in standalone) or "none",
                                          ", %d chains (%s)" % (len(chains), ", ".join(c["name"] for c in chains)) if chains else ""))
    return effects



# ---------------------------------------------------------------- chains

KNOB_COUNT = 6          # physical knobs; a mapping on knob 7 to 12 is on the shifted bank (see effect_chain.h)
MAX_KNOB = 12
SLOT_ENABLE = "SLOT_ENABLE"


def read_chains(path=None):
    """The chains of chains.json: [{name, slots: [{tag, effect}], mappings: [{slot, param, knob (0-11 or null), cc (0-127 or null)}], primary, toggle: [slot, ...]}]"""
    path = path or CHAINS
    if not os.path.exists(path):
        return []
    with open(path) as f:
        return json.load(f).get("chains", [])


def write_chains(chains, path=None):
    path = path or CHAINS
    with open(path, "w") as f:
        json.dump({"_note": "Effect chains, edited with tools/effects.py (key c in the interactive mode). knob is 0-5 for the six knobs and 6-11 for the same knobs while the alternate footswitch's shift bank is on; null for none.", "chains": chains}, f, indent=1)
        f.write("\n")


def effect_params(effect):
    """The identifiers of the effect's parameters: the members of `enum Param { ... }` in its header, in order (what a chain mapping names), without the count."""
    path = os.path.join(ROOT, "Effect-Modules", effect["header"])
    if not os.path.exists(path):
        return []
    text = open(path, errors="replace").read()
    mt = re.search(r"enum\s+Param\s*\{([^}]*)\}", text)
    if not mt:
        return []
    names = []
    for item in re.sub(r"//[^\n]*|/\*.*?\*/", "", mt.group(1), flags=re.S).split(","):
        name = item.split("=")[0].strip()
        if name and name != "PARAM_COUNT" and not name.endswith("_COUNT"):
            names.append(name)
    return names


def uses_cloudseed_engine(effect, m):
    return any(c == "cloudseed_pool" for c in effect.get("requires", []))


def chain_blocked(effect, m):
    """Why an effect cannot be a slot of a chain (None if it can)."""
    if uses_cloudseed_engine(effect, m):
        return "its reverb engine is sized for one instance standalone (see effect_chain.cpp)"
    if not effect_params(effect):
        return "it has no parameter enum to map"
    return None


def one_instance_only(effect, mem):
    """True if a second instance (a chain slot next to the standalone effect, or in two chains) would share its buffers: it keeps memory in SDRAM or says single-instance."""
    f = (mem or {}).get("effects", {}).get(effect["id"], {})
    return bool(f.get("sdram", 0)) or "single-instance" in effect.get("note", "")


def chain_members(chains):
    return {s["effect"] for c in chains for s in c.get("slots", [])}


def chain_problems(chain, enabled, chains, m, mem=None):
    """What is wrong with one chain (a list of messages; empty if it can be built)."""
    problems = []
    name = chain.get("name", "")
    if not name.strip():
        problems.append("the chain has no name")
    slots = chain.get("slots", [])
    if len(slots) < 2:
        problems.append("a chain needs at least two effects")
    tags = set()
    for n, sl in enumerate(slots):
        e = m["by_id"].get(sl.get("effect"))
        if e is None:
            problems.append("slot %d: unknown effect %s" % (n + 1, sl.get("effect")))
            continue
        why = chain_blocked(e, m)
        if why:
            problems.append("slot %d: %s cannot be chained: %s" % (n + 1, e["id"], why))
        if not sl.get("tag", "").strip():
            problems.append("slot %d: no tag" % (n + 1))
        elif len(sl["tag"]) > 2:
            problems.append("slot %d: the tag %r is longer than 2 characters" % (n + 1, sl["tag"]))
        elif sl["tag"] in tags:
            problems.append("slot %d: the tag %r is used twice" % (n + 1, sl["tag"]))
        tags.add(sl.get("tag"))
        if one_instance_only(e, mem):
            if e["id"] in enabled:
                problems.append("%s is also enabled on its own: it can only exist once (disable it, the chain has it)" % e["id"])
            others = sum(1 for c in chains if c is not chain for o in c.get("slots", []) if o.get("effect") == e["id"])
            if others or sum(1 for o in slots if o.get("effect") == e["id"]) > 1:
                problems.append("%s is in more than one slot or chain: it can only exist once" % e["id"])
    if slots and not (0 <= chain.get("primary", 0) < len(slots)):
        problems.append("the primary slot %s does not exist" % (chain.get("primary", 0) + 1))
    for t in chain.get("toggle", []):
        if not (0 <= t < len(slots)):
            problems.append("the footswitch toggles slot %d, which does not exist" % (t + 1))
    seen_knobs, seen_cc = {}, {}
    for k, mp in enumerate(chain.get("mappings", [])):
        sl = mp.get("slot", -1)
        if not (0 <= sl < len(slots)):
            problems.append("mapping %d: slot %s does not exist" % (k + 1, sl + 1))
            continue
        e = m["by_id"].get(slots[sl].get("effect"))
        if e is not None and mp.get("param") != SLOT_ENABLE and mp.get("param") not in effect_params(e):
            problems.append("mapping %d: %s has no parameter %s" % (k + 1, e["id"], mp.get("param")))
        knob = mp.get("knob")
        if knob is not None:
            if not (0 <= knob < MAX_KNOB):
                problems.append("mapping %d: knob %d is not 1 to 12" % (k + 1, knob + 1))
            elif knob in seen_knobs:
                problems.append("mapping %d: knob %d is already used by mapping %d" % (k + 1, knob + 1, seen_knobs[knob] + 1))
            seen_knobs[knob] = k
        cc = mp.get("cc")
        if cc is not None:
            if not (0 <= cc <= 127):
                problems.append("mapping %d: MIDI CC %d is not 0 to 127" % (k + 1, cc))
            elif cc in seen_cc:
                problems.append("mapping %d: MIDI CC %d is already used by mapping %d" % (k + 1, cc, seen_cc[cc] + 1))
            seen_cc[cc] = k
    return problems


def all_chain_problems(enabled, chains, m, mem=None):
    out = []
    names = {}
    for c in chains:
        names[c.get("name")] = names.get(c.get("name"), 0) + 1
    for c in chains:
        for p in chain_problems(c, enabled, chains, m, mem):
            out.append("chain %s: %s" % (c.get("name", "?"), p))
        if names.get(c.get("name"), 0) > 1:
            out.append("chain %s: the name is used twice" % c.get("name"))
    return out


def effective_enabled(enabled, chains):
    """The effects that are compiled: the standalone ones and every chain slot."""
    return set(enabled) | chain_members(chains)


def cstr(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def chain_code(chain, m):
    """The C++ expression `new EffectChain(...)` of one chain."""
    slots = chain["slots"]
    slot_text = ", ".join("{%s, new %s()}" % (cstr(sl["tag"]), m["by_id"][sl["effect"]]["class"]) for sl in slots)
    maps = []
    for mp in chain.get("mappings", []):
        e = m["by_id"][slots[mp["slot"]]["effect"]]
        param = "EffectChain::SLOT_ENABLE" if mp["param"] == SLOT_ENABLE else "%s::%s" % (e["class"], mp["param"])
        maps.append("{%d, %s, %d, %d}" % (mp["slot"], param, -1 if mp.get("knob") is None else mp["knob"], -1 if mp.get("cc") is None else mp["cc"]))
    lines = ["new EffectChain(", "    %s," % cstr(chain["name"]), "    {%s}," % slot_text, "    {%s}," % (",\n     ".join(maps))]
    lines.append("    /* primarySlot */ %d," % chain.get("primary", 0))
    lines.append("    /* footswitchTogglesSlots */ {%s}")
    lines[-1] = lines[-1] % ", ".join(str(t) for t in chain.get("toggle", []))
    return "\n".join(lines) + "),"



# ---------------------------------------------------------------- display text

NAME_BIG = 10   # characters that fit the biggest font (11 pixels wide) in the 110 pixels of a name row; longer names are drawn smaller (UI/fit_text.h)
NAME_MAX = 22   # characters that fit the smallest font (5 pixels wide); longer names are cut with a "~"


def scan_texts(source_text):
    """(parameter names, choices) found in an effect's source: the `name : "..."` of its parameter metadata and the strings of its `const char *xxx[] = {...}` arrays."""
    names = re.findall(r'name\s*:\s*"([^"]*)"', source_text)
    choices = []
    for arr in re.findall(r'const char \*\s*\w+\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}', source_text):
        choices += re.findall(r'"([^"]*)"', arr)
    return names, choices


def lint_texts(m, chains=None):
    """The texts that do not fit the big font: [{effect, kind, text, length, result}], result 'smaller' (drawn in a smaller font) or 'cut' (longer than even the smallest font shows)."""
    found = []
    per_effect = {}
    for e in m["effects"]:
        names, choices = [], []
        for src in e["sources"]:
            path = os.path.join(ROOT, src)
            if src.endswith("_module.cpp") and os.path.exists(path):
                n, c = scan_texts(open(path, errors="replace").read())
                names += n
                choices += c
        per_effect[e["id"]] = names
        for kind, texts in (("parameter", names), ("choice", choices)):
            for t in texts:
                if len(t) > NAME_BIG:
                    found.append({"effect": e["id"], "kind": kind, "text": t, "length": len(t), "result": "cut" if len(t) > NAME_MAX else "smaller"})
    for c in chains or []:  # a chain's parameters are "<tag> <name>"
        for sl in c.get("slots", []):
            for t in per_effect.get(sl.get("effect"), []):
                combined = "%s %s" % (sl.get("tag", ""), t)
                if len(combined) > NAME_BIG:
                    found.append({"effect": "chain " + c.get("name", "?"), "kind": "parameter", "text": combined, "length": len(combined), "result": "cut" if len(combined) > NAME_MAX else "smaller"})
    return found


def cmd_lint(args):
    m = manifest()
    found = lint_texts(m, read_chains())
    cut = [f for f in found if f["result"] == "cut"]
    print("%d texts are longer than %d characters (drawn in a smaller font), %d longer than %d (cut with a ~)\n" % (len(found) - len(cut), NAME_BIG, len(cut), NAME_MAX))
    for f in sorted(found, key=lambda f: (f["result"] != "cut", f["effect"], f["text"])):
        print("%-8s %-18s %-9s %2d  %s" % (f["result"], f["effect"], f["kind"], f["length"], f["text"]))
    if cut:
        sys.exit(1)


# ---------------------------------------------------------------- memory

def region_of_section(name):
    """Which memory a section of an object file ends up in (see the linker script and nam_a2_sections.lds)."""
    if name.startswith(".sdram_bss"):
        return "sdram"
    if name.startswith(".sram_d2_bss"):
        return "ram_d2"
    if name.startswith(".sram1_bss"):
        return None  # the audio DMA buffers: in RAM_D2_DMA (the first 32 KB of D2), a separate region that does not touch the heap
    if name.startswith((".bss", ".data", ".dtcmram_bss", ".noinit")):
        return "dtcm"
    if name.startswith((".text", ".rodata", ".init_array", ".fini_array", ".ARM")):
        return "sram"
    return None


def object_sizes(path):
    """Bytes per memory region in one object file."""
    out = subprocess.run(["arm-none-eabi-size", "-A", path], capture_output=True, text=True)
    if out.returncode != 0:
        return None
    sizes = {r: 0 for r in REGIONS}
    seen, skip = set(), False  # an archive can hold the same member twice (`ar -r` without a clean build): count each once
    for line in out.stdout.splitlines():
        parts = line.split()
        if line.rstrip().endswith("):") or (line.rstrip().endswith(":") and "(" not in line):
            name = line.rstrip()
            skip = name in seen
            seen.add(name)
            continue
        if skip:
            continue
        if len(parts) >= 2 and parts[0].startswith(".") and parts[1].isdigit():
            r = region_of_section(parts[0])
            if r:
                sizes[r] += int(parts[1])
    return sizes


def sources_of(effect, m):
    s = list(effect["sources"])
    for c in effect.get("requires", []):
        s += m["components"][c].get("sources", [])
    return s


def measure(build_dir):
    """Updates the static memory figures of the effects whose object files are in build_dir (from `arm-none-eabi-size -A`)."""
    m = manifest()
    mem = load_json(MEMORY)
    updated = []
    for e in m["effects"]:
        total = {r: 0 for r in REGIONS}
        found = 0
        for s in e["sources"]:  # the effect's own sources only; the components are accounted for separately
            obj = os.path.join(build_dir, os.path.splitext(os.path.basename(s))[0] + ".o")
            sz = object_sizes(obj) if os.path.exists(obj) else None
            if sz is None:
                continue
            found += 1
            for r in REGIONS:
                total[r] += sz[r]
        if found == len(e["sources"]):
            entry = mem["effects"].setdefault(e["id"], {})
            entry.update(total)
            entry["static_source"] = "object files (arm-none-eabi-size -A)"
            updated.append(e["id"])
    # components
    for cname, c in m["components"].items():
        srcs = c.get("sources", []) + c.get("c_sources", [])
        paths = [] if c.get("measure") == "libs" else [os.path.join(build_dir, os.path.splitext(os.path.basename(s))[0] + ".o") for s in srcs]  # "libs": the sources are accounted for by hand (the pool's size is set by the build)
        paths += [os.path.join(ROOT, l) for l in c.get("libs", [])]
        if not paths or c.get("measure") == "manual":
            continue
        total = {r: 0 for r in REGIONS}
        ok = True
        for obj in paths:
            sz = object_sizes(obj) if os.path.exists(obj) else None
            if sz is None:
                ok = False
                break
            for r in REGIONS:
                total[r] += sz[r]
        if ok:
            mem["components"].setdefault(cname, {}).update(total)
    with open(MEMORY, "w") as f:
        json.dump(mem, f, indent=1)
    print("updated the static figures of: %s" % (", ".join(updated) or "nothing (no object files found: build the effects first, e.g. `tools/effects.py preset everything` and `make -k`)"))


def calibrate(build_dir):
    """Sets the memory the firmware uses without any effect ("base": the framework, the UI, libDaisy, ...) from the last build: the ELF's size minus what the effects of the
    current selection were measured to cost (so it needs a build of the current selection, and `measure` run on objects of the same compiler)."""
    m = manifest()
    mem = load_json(MEMORY)
    enabled, options = read_config()
    effects, comps = resolve(effective_enabled(enabled, read_chains()), options, m)
    elf = os.path.join(build_dir, "guitarpedal.elf")
    total = object_sizes(elf)
    if total is None:
        sys.exit("no %s: build the current selection first" % elf)
    ids = [e["id"] for e in effects]
    used, heap, notes = budget(ids, m, mem)
    for r in REGIONS:
        mem["base"][r] = max(0, total[r] - (used[r] - (heap if r == "ram_d2" else 0) - mem["base"].get(r, 0)))
    mem["base"]["_calibrated_with"] = "the build of: %s" % (", ".join(ids) or "no effects")
    with open(MEMORY, "w") as f:
        json.dump(mem, f, indent=1)
    print("base (without effects): " + ", ".join("%s %d KB" % (r, mem["base"][r] // 1024) for r in REGIONS) + "  (from %s)" % elf)


def budget(effect_ids, m, mem, options=None):
    """Memory use of a set of effects: totals per region plus warnings."""
    used = {r: mem["base"].get(r, 0) for r in REGIONS}
    heap = mem["base"].get("heap", 0)
    notes = []
    comps = []
    for i in effect_ids:
        e = m["by_id"][i]
        for c in e.get("requires", []):
            if c not in comps:
                comps.append(c)
        figures = mem["effects"].get(i)
        if not figures:
            notes.append("%s: no memory figures (run `measure`)" % i)
            continue
        for r in REGIONS:
            used[r] += figures.get(r, 0)
        if figures.get("heap") is None:
            notes.append("%s: heap not measured (counted as 0)" % i)
        else:
            heap += figures["heap"]
    for c in comps:
        if c == "cloudseed_pool":  # its size follows the number of engines (see cloudseed_pool.cpp)
            engines = sum(1 for i in effect_ids if c in m["by_id"][i].get("requires", []))
            used["sdram"] += POOL_BYTES[min(max(engines, 1), 3)] + mem["components"].get(c, {}).get("sdram", 0)  # plus the engine library's lookup tables in SDRAM
            used["sram"] += mem["components"].get(c, {}).get("sram", 0)
            used["dtcm"] += mem["components"].get(c, {}).get("dtcm", 0)
            continue
        cf = mem["components"].get(c, {})
        for r in REGIONS:
            used[r] += cf.get(r, 0)
    used["ram_d2"] += heap
    return used, heap, notes


def verdict(used, mem):
    size = mem["regions"]
    problems = []
    tight = []
    for r in REGIONS:
        limit = size[r] - (mem.get("reserve", {}).get(r, 0))
        if used[r] > size[r]:
            problems.append("%s over (%d KB of %d KB)" % (REGION_NAMES[r], used[r] // 1024, size[r] // 1024))
        elif used[r] > limit * 0.92:
            tight.append("%s tight (%d%%)" % (REGION_NAMES[r], 100 * used[r] // size[r]))
    if problems:
        return "does not fit", problems
    if tight:
        return "tight", tight
    return "fits", []


def cmd_report(args):
    m = manifest()
    mem = load_json(MEMORY)
    enabled, options = read_config(args.config or CONFIG)
    effects, comps = resolve(effective_enabled(enabled, read_chains()), options, m)  # the chains' effects are built too
    ids = [e["id"] for e in effects]
    used, heap, notes = budget(ids, m, mem)
    print("effects: %s" % (", ".join(ids) or "none"))
    print("components: %s\n" % (", ".join(comps) or "none"))
    print("%-24s %10s %10s %6s" % ("region", "used", "size", "%"))
    for r in REGIONS:
        size = mem["regions"][r]
        print("%-24s %9d K %9d K %5d%%" % (REGION_NAMES[r], used[r] // 1024, size // 1024, 100 * used[r] // size))
    static_d2 = used["ram_d2"] - heap
    print("\nRAM_D2: %d KB of static data leaves %d KB for the heap; the effects are estimated to need %d KB of it (estimates, see docs/PERFORMANCE_AND_MEMORY.md): %d KB to spare" % (static_d2 // 1024, (mem["regions"]["ram_d2"] - static_d2) // 1024, heap // 1024, (mem["regions"]["ram_d2"] - used["ram_d2"]) // 1024))
    v, why = verdict(used, mem)
    print("\nverdict: %s%s" % (v, (" (" + "; ".join(why) + ")") if why else ""))
    for n in notes:
        print("note: " + n)
    print("note: static figures come from the object files, which are slightly larger than what is linked; heap figures are host counts except the ones the pedal has measured (see heap_source in effects-memory.json: Jupiter, Neptune, the framework).")


def cmd_observe(args):
    """Records what was seen on the pedal for the current selection (Show Debug's heap figures, whether it booted) in effects-memory.json, where `matrix` and `report` show it."""
    m = manifest()
    mem = load_json(MEMORY)
    enabled, options = read_config()
    effects, comps = resolve(effective_enabled(enabled, read_chains()), options, m)
    ids = sorted(e["id"] for e in effects)
    entry = {"effects": ids, "result": args.result}
    if args.heap_used is not None:
        entry["heap_used_kb"] = args.heap_used
    if args.heap_free is not None:
        entry["heap_free_kb"] = args.heap_free
    if args.note:
        entry["note"] = args.note
    used, heap, notes = budget(ids, m, mem)
    entry["estimated_heap_kb"] = heap // 1024  # what the estimate said, to compare
    if args.set_base_heap and args.heap_used is not None:
        # what the framework itself needs: the heap used minus what the effects are estimated to need (it is the same whatever is selected, up to the menus of the active effect)
        mem["base"]["heap"] = max(0, args.heap_used * 1024 - (heap - mem["base"].get("heap", 0)))
        entry["estimated_heap_kb"] = (heap - mem["base"].get("heap", 0) + mem["base"]["heap"]) // 1024
        print("base heap (the framework, without effects) set to %d KB" % (mem["base"]["heap"] // 1024))
    observed = [o for o in mem.get("observed", []) if sorted(o["effects"]) != ids]  # a new observation of the same selection replaces the old one
    observed.append(entry)
    mem["observed"] = observed
    with open(MEMORY, "w") as f:
        json.dump(mem, f, indent=1)
    print("recorded for %s: %s%s" % (", ".join(ids) or "no effects", args.result, (" (heap used %s K, free %s K; the estimate was %d K)" % (args.heap_used, args.heap_free, heap // 1024)) if args.heap_used is not None else ""))


def cmd_matrix(args):
    m = manifest()
    mem = load_json(MEMORY)
    ids = args.ids or [e["id"] for e in m["effects"] if e["id"] in read_config()[0]]
    if len(ids) > 12:
        sys.exit("too many effects for a matrix of all combinations (%d); name at most 12" % len(ids))
    ids = [i for i in ids]
    observed = {frozenset(o["effects"]): o for o in mem.get("observed", [])}
    print("| Combination | RAM_D2 | SRAM | DTCM | SDRAM | Estimate | Observed on the pedal |\n|---|---|---|---|---|---|---|")
    for k in range(1, len(ids) + 1):
        for combo in itertools.combinations(ids, k):
            used, heap, notes = budget(list(combo), m, mem)
            v, why = verdict(used, mem)
            o = observed.get(frozenset(combo))
            seen = ""
            if o:
                seen = "**%s**" % o["result"]
                if o.get("heap_used_kb") is not None:
                    seen += " (heap used %d K, free %s K)" % (o["heap_used_kb"], o.get("heap_free_kb", "?"))
            print("| %s | %d KB | %d KB | %d KB | %.1f MB | %s | %s |" % (" + ".join(combo), used["ram_d2"] // 1024, used["sram"] // 1024, used["dtcm"] // 1024, used["sdram"] / 1048576, v, seen))


# ---------------------------------------------------------------- metadata

def source_files(effect, m):
    """The effect's own files: its sources, its header, and the README of its directory if it has one."""
    files = [s for s in effect["sources"]]
    files.append("Effect-Modules/" + effect["header"])
    return files


def copyright_lines(effect, m, limit=3):
    """Copyright and 'adapted from' comment lines found in the first 40 lines of the effect's source files (an inference from text, not a guarantee)."""
    found = []
    pattern = re.compile(r"copyright|\(c\)|©|\ufffd|adapted from|ported from|based on|taken from", re.I)  # (\ufffd: a (c) sign in a Latin-1 file)
    boilerplate = re.compile(r"above copyright notice|copyright holders|copyright owner|copyright notice", re.I)  # the licence text itself, not who wrote it
    for rel in source_files(effect, m):
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        with open(path, errors="replace") as f:
            for n, line in enumerate(f):
                if n >= 40:
                    break
                stripped = line.strip()
                if not stripped.startswith(("//", "/*", "*", "#")):
                    continue  # comments only
                text = stripped.lstrip("/*# ").strip().replace("\ufffd", "(c)")
                if text and pattern.search(text) and not boilerplate.search(text) and text not in found:
                    found.append(text[:200])
    return found[:limit]


_author_cache = {}


def git_authors(effect, m, limit=3):
    """[(name, commits)] of the people who committed the effect's source files, most commits first (names in author_aliases are merged). Empty without git."""
    key = effect["id"]
    if key in _author_cache:
        return _author_cache[key][:limit]
    aliases = m.get("author_aliases", {})
    counts = {}
    try:
        out = subprocess.run(["git", "log", "--format=%aN", "--"] + source_files(effect, m), cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))), capture_output=True, text=True, timeout=10)
        for name in out.stdout.splitlines():
            name = aliases.get(name.strip(), name.strip())
            if name:
                counts[name] = counts.get(name, 0) + 1
    except (OSError, subprocess.SubprocessError):
        pass
    _author_cache[key] = sorted(counts.items(), key=lambda kv: -kv[1])
    return _author_cache[key][:limit]


def effect_info(effect, m, mem=None):
    """Everything the tool knows about an effect, as (label, text) lines."""
    lines = [("effect", "%s (class %s, %s)" % (effect["id"], effect["class"], effect["header"]))]
    if effect.get("note"):
        lines.append(("note", effect["note"]))
    if effect.get("origin"):
        lines.append(("origin", effect["origin"]))
    lines.append(("license", "GPL-3.0-or-later" if effect.get("license") == "gpl" else "as the rest of the repository (MIT) unless the origin says otherwise"))
    cr = copyright_lines(effect, m)
    if cr:
        for k, c in enumerate(cr):
            lines.append(("in the source" if k == 0 else "", c))
    authors = git_authors(effect, m)
    if authors:
        lines.append(("committed by", ", ".join("%s (%d)" % a for a in authors)))
    if effect.get("requires"):
        lines.append(("needs", ", ".join(effect["requires"])))
    if effect.get("conflicts"):
        lines.append(("not with", ", ".join(effect["conflicts"])))
    readme = os.path.join(os.path.dirname(effect["sources"][0]), "README.md")
    cand = [os.path.join(os.path.dirname(effect["sources"][0]), d, "README.md") for d in (effect["id"].capitalize(),)]
    for r in [readme] + cand:
        if os.path.exists(os.path.join(ROOT, r)):
            lines.append(("readme", r))
            break
    lines.append(("sources", ", ".join(effect["sources"])))
    if mem:
        f = mem["effects"].get(effect["id"])
        if f:
            lines.append(("memory", "code %d K, DTCM %d K, RAM_D2 %d K, SDRAM %.1f M, heap %s" % (f.get("sram", 0) // 1024, f.get("dtcm", 0) // 1024, f.get("ram_d2", 0) // 1024, f.get("sdram", 0) / 1048576,
                                                                                                  ("%d K" % (f["heap"] // 1024)) if f.get("heap") is not None else "?")))
    return lines


def cmd_info(args):
    m = manifest()
    mem = load_json(MEMORY)
    for i in args.ids:
        if i not in m["by_id"]:
            sys.exit("unknown effect: %s (see `list`)" % i)
        for label, text in effect_info(m["by_id"][i], m, mem):
            print("%-14s %s" % (label, text))
        print()


# ---------------------------------------------------------------- selection

def cmd_list(args):
    m = manifest()
    enabled, options = read_config()
    mem = load_json(MEMORY)
    for e in m["effects"]:
        f = mem["effects"].get(e["id"], {})
        cost = ("RAM_D2 %4d K, DTCM %4d K, SDRAM %5.1f M, code %4d K, heap %s" % (f.get("ram_d2", 0) // 1024, f.get("dtcm", 0) // 1024, f.get("sdram", 0) / 1048576, f.get("sram", 0) // 1024,
                "%d K" % (f["heap"] // 1024) if f.get("heap") is not None else "?")) if f else "(no figures)"
        print("[%s] %-18s %s%s" % ("x" if e["id"] in enabled else " ", e["id"], cost, "  GPL" if e.get("license") == "gpl" else ""))


def cmd_change(args, on):
    m = manifest()
    enabled, options = read_config()
    for i in args.ids:
        if i not in m["by_id"]:
            sys.exit("unknown effect: %s (see `list`)" % i)
        (enabled.add if on else enabled.discard)(i)
    resolve(enabled, options, m)  # refuses a selection that cannot be built (GPL without ALLOW_GPL, conflicts) before anything is written
    write_config(enabled, options, m)
    generate()


def cmd_preset(args):
    m = manifest()
    path = os.path.join(PRESETS, args.name + ".config")
    if args.name == "everything":
        enabled = {e["id"] for e in m["effects"]}
        options = {"ALLOW_GPL": True}
    elif os.path.exists(path):
        enabled, options = read_config(path)
    else:
        sys.exit("no such preset: %s (presets: %s)" % (args.name, ", ".join(sorted(f[:-7] for f in os.listdir(PRESETS) if f.endswith(".config")) + ["everything"])))
    write_config(enabled, options, m)
    generate()


def cmd_tui(args):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import effects_tui
    effects_tui.run(sys.modules[__name__])


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("tui", help="the interactive full-screen selection (the default when run in a terminal)")
    sub.add_parser("list")
    sub.add_parser("lint", help="parameter names and choices that are too long for the display (drawn smaller, or cut)")
    s = sub.add_parser("info", help="what is known about an effect: origin, copyright lines, who committed it, memory"); s.add_argument("ids", nargs="+")
    s = sub.add_parser("preset"); s.add_argument("name")
    s = sub.add_parser("enable"); s.add_argument("ids", nargs="+")
    s = sub.add_parser("disable"); s.add_argument("ids", nargs="+")
    sub.add_parser("generate")
    s = sub.add_parser("report"); s.add_argument("--config")
    s = sub.add_parser("measure"); s.add_argument("--build-dir", default=os.path.join(ROOT, "build"))
    s = sub.add_parser("calibrate"); s.add_argument("--build-dir", default=os.path.join(ROOT, "build"))
    s = sub.add_parser("matrix"); s.add_argument("ids", nargs="*")
    s = sub.add_parser("observe", help="record what was seen on the pedal for the current selection"); s.add_argument("result", help='for example "boots and runs" or "does not boot"')
    s.add_argument("--heap-used", type=int, help="the heap's used KB in Show Debug"); s.add_argument("--heap-free", type=int, help="the heap's free KB in Show Debug"); s.add_argument("--note"); s.add_argument("--set-base-heap", action="store_true", help="also set the framework's own heap (the base) from this observation: heap used minus the effects' estimates")
    a = p.parse_args()
    if a.cmd is None:
        if sys.stdin.isatty() and sys.stdout.isatty():
            a.cmd = "tui"
        else:
            p.print_help()
            return
    {"tui": cmd_tui, "lint": cmd_lint, "info": cmd_info, "list": cmd_list, "preset": cmd_preset, "enable": lambda x: cmd_change(x, True), "disable": lambda x: cmd_change(x, False), "generate": lambda x: generate(),
     "report": cmd_report, "measure": lambda x: measure(x.build_dir), "calibrate": lambda x: calibrate(x.build_dir), "matrix": cmd_matrix, "observe": cmd_observe}[a.cmd](a)


if __name__ == "__main__":
    main()
