"""The interactive, full-screen effect selection of tools/effects.py (curses; Python's standard library only).

  Up/Down (or k/j), PgUp/PgDn, Home/End   move in the list of effects
  Space                                   select / deselect the effect under the cursor
  s                                       save (writes effects.config and generates the build files)
  p                                       start from a preset
  a                                       allow / forbid the GPL-3.0 effects (ALLOW_GPL)
  c                                       effect chains: several effects combined into one menu entry
  x                                       clear everything: no effects, no chains, GPL forbidden (asks first; nothing is written until saved)
  i                                       about the effect under the cursor: origin, copyright lines, who committed it
  r                                       the memory budget in detail
  h                                       help
  q                                       quit (asks about unsaved changes)

The state and what it computes (`Selector`) do not touch the terminal, so it is tested without one (tests/test_effects_tui.py).
"""
import copy
import curses
import os

REGION_SHORT = {"dtcm": "DTCM", "ram_d2": "RAM_D2", "sram": "Code", "sdram": "SDRAM"}

HELP = [
    "Choose the effects that are built into the firmware.",
    "",
    "  Up / Down, k / j     move          PgUp / PgDn, Home / End   jump",
    "  Space                select or deselect the effect (a GPL effect asks first to allow the GPL effects)",
    "  s                    save: writes effects.config and the generated build files",
    "  p                    start from a preset (configs/*.config, everything)",
    "  a                    allow / forbid the GPL-3.0 effects (ALLOW_GPL)",
    "  c                    effect chains: several effects combined into one menu entry",
    "                       ([c] in the list is an effect that is only built for a chain)",
    "  x                    clear everything (no effects, no chains, GPL forbidden), to start from scratch",
    "  i                    about the effect: origin, copyright lines, who committed it",
    "  r                    the memory budget in detail",
    "  h                    this help               q   quit",
    "",
    "The status bar is an estimate of what the selection needs:",
    "  DTCM and RAM_D2 are where the effects' variables and the heap are, Code is the",
    "  program (SRAM), SDRAM is the external memory. The percentages are of each memory's",
    "  size. 'fits', 'tight' or 'does NOT fit' is the verdict (RAM_D2 includes the heap,",
    "  which is only an estimate). Run `tools/effects.py measure` after a build to update",
    "  the figures. A '*' means unsaved changes.",
    "",
    "A selection that cannot be built (conflicting effects, GPL effects without",
    "ALLOW_GPL) is shown in red and cannot be saved.",
    "",
    "Press any key to close.",
]


class Selector:
    """The selection and what follows from it: no curses in here."""

    def __init__(self, fx):
        self.fx = fx
        self.m = fx.manifest()
        self.mem = fx.load_json(fx.MEMORY)
        self.enabled, self.options = fx.read_config()
        self.chains = copy.deepcopy(fx.read_chains())
        self.effects = self.m["effects"]
        self.cursor = next((i for i, e in enumerate(self.effects) if e["id"] in self.enabled), 0)  # starts on the first selected effect
        self.top = 0
        self.dirty = False
        self.message = ""

    # -- selection
    def current(self):
        return self.effects[self.cursor]

    def move(self, delta, page=1):
        self.cursor = max(0, min(len(self.effects) - 1, self.cursor + delta * page))

    def toggle(self, effect_id=None):
        i = effect_id or self.current()["id"]
        if i in self.enabled:
            self.enabled.discard(i)
        else:
            self.enabled.add(i)
        self.dirty = True
        self.message = ""

    def needs_gpl(self, effect_id):
        """Whether the effect (or a component it needs) is GPL-3.0-or-later."""
        e = self.m["by_id"][effect_id]
        return e.get("license") == "gpl" or any(self.m["components"][c].get("license") == "gpl" for c in e.get("requires", []))

    def toggle_gpl(self):
        self.options["ALLOW_GPL"] = not self.options.get("ALLOW_GPL")
        self.dirty = True

    def presets(self):
        names = []
        if os.path.isdir(self.fx.PRESETS):
            names = sorted(f[:-7] for f in os.listdir(self.fx.PRESETS) if f.endswith(".config"))
        return names + ["everything"]

    def load_preset(self, name):
        if name == "everything":
            self.enabled = {e["id"] for e in self.effects} - {"reverb_delay"}
            self.options["ALLOW_GPL"] = True
        else:
            self.enabled, options = self.fx.read_config(os.path.join(self.fx.PRESETS, name + ".config"))
            # (the chains stay.) A preset can allow the GPL effects but does not forbid them again: "ALLOW_GPL=n" in it means only that it has none,
            # and it used to switch off what was allowed with "a" a moment before.
            if options.get("ALLOW_GPL"):
                self.options["ALLOW_GPL"] = True
        self.dirty = True
        self.message = "preset %s loaded (not saved)" % name

    def clear(self):
        """Everything off: no effects, no chains, the GPL effects forbidden again (not saved until save())."""
        self.enabled = set()
        self.chains = []
        self.options["ALLOW_GPL"] = False
        self.cursor = 0
        self.top = 0
        self.dirty = True
        self.message = "cleared (not saved): select the effects, then s to save"

    # -- what the selection needs
    def built(self):
        """What is compiled: the standalone effects and every effect that is a slot of a chain."""
        return self.fx.effective_enabled(self.enabled, self.chains)

    def problems(self):
        if not self.built():
            return ["nothing is selected (the firmware needs at least one effect)"]
        return self.fx.selection_problems(self.built(), self.options, self.m) + self.fx.all_chain_problems(self.enabled, self.chains, self.m, self.mem)

    def ids(self):
        built = self.built()
        return [e["id"] for e in self.effects if e["id"] in built]

    def in_chain(self, effect_id):
        return effect_id in self.fx.chain_members(self.chains)

    def usage(self):
        """(used per region, heap, verdict, reasons)"""
        used, heap, notes = self.fx.budget(self.ids(), self.m, self.mem)
        verdict, why = self.fx.verdict(used, self.mem)
        return used, heap, verdict, why

    def percent(self, used, region):
        return 100 * used[region] // self.mem["regions"][region]

    def status(self):
        """The text of the status bar and its severity (0 fine, 1 tight, 2 does not fit or cannot be built)."""
        used, heap, verdict, why = self.usage()
        parts = ["%s %d%%" % (REGION_SHORT[r], self.percent(used, r)) for r in self.fx.REGIONS]
        sev = {"fits": 0, "tight": 1}.get(verdict, 2)
        text = "  ".join(parts) + "  | " + {"fits": "fits", "tight": "tight"}.get(verdict, "does NOT fit")
        if self.problems():
            text += "  | cannot be built"
            sev = 2
        if self.options.get("ALLOW_GPL"):
            text += "  | GPL allowed"
        count = "%d effects" % len(self.ids()) + (", %d chains" % len(self.chains) if self.chains else "")
        text = ("* " if self.dirty else "") + count + "  " + text
        return text, sev

    def effect_cost(self, e):
        f = self.mem["effects"].get(e["id"], {})
        return {"code": f.get("sram", 0), "dtcm": f.get("dtcm", 0), "ram_d2": f.get("ram_d2", 0), "sdram": f.get("sdram", 0), "heap": f.get("heap")}

    def detail(self, e):
        """The line under the list: what the effect under the cursor is and needs."""
        bits = []
        if e.get("note"):
            bits.append(e["note"])
        if e.get("license") == "gpl":
            bits.append("GPL-3.0-or-later")
        if e.get("origin"):
            bits.append("from " + e["origin"])
        else:
            authors = self.fx.git_authors(e, self.m, 2)
            if authors:
                bits.append("committed by " + ", ".join(a[0] for a in authors))
        if e.get("requires"):
            bits.append("needs " + ", ".join(e["requires"]))
        if e.get("conflicts"):
            bits.append("not with " + ", ".join(e["conflicts"]))
        return "; ".join(bits)

    def info_lines(self, width=76):
        """All that is known about the effect under the cursor, wrapped to the width of the popup."""
        import textwrap
        out = []
        for label, text in self.fx.effect_info(self.current(), self.m, self.mem):
            wrapped = textwrap.wrap(text, width - 15) or [""]
            for k, w in enumerate(wrapped):
                out.append("%-14s %s" % (label if k == 0 else "", w))
        out += ["", "Press any key to close."]
        return out

    def report_lines(self):
        used, heap, verdict, why = self.usage()
        lines = ["%-24s %10s %10s %6s" % ("region", "used", "size", "%")]
        for r in self.fx.REGIONS:
            size = self.mem["regions"][r]
            lines.append("%-24s %9d K %9d K %5d%%" % (self.fx.REGION_NAMES[r], used[r] // 1024, size // 1024, 100 * used[r] // size))
        static_d2 = used["ram_d2"] - heap
        lines += ["", "RAM_D2: %d KB static data, %d KB heap (estimate), %d KB to spare" % (static_d2 // 1024, heap // 1024, (self.mem["regions"]["ram_d2"] - used["ram_d2"]) // 1024),
                  "", "verdict: %s%s" % (verdict, (" (" + "; ".join(why) + ")") if why else "")]
        lines += ["problem: " + p for p in self.problems()]
        lines += ["", "Press any key to close."]
        return lines

    # -- saving
    def save(self):
        problems = self.problems()
        if problems:
            self.message = "cannot save: " + "; ".join(problems) + ("  (a: allow the GPL effects)" if any("ALLOW_GPL" in x for x in problems) else "")
            return False
        self.fx.write_config(self.enabled, self.options, self.m)
        self.fx.write_chains(self.chains)
        self.fx.generate(quiet=True)
        self.dirty = False
        self.message = "saved: effects.config, chains.json and the generated build files"
        return True


class ChainEditor:
    """One chain being edited. Slots and mappings refer to each other by index, so every change that moves or removes a slot fixes the references: no curses in here."""

    def __init__(self, sel, chain):
        self.sel = sel
        self.fx = sel.fx
        self.chain = chain
        self.cursor = 0
        self.message = ""

    # -- the rows of the editor: (kind, index, text)
    def slot_text(self, i):
        sl = self.chain["slots"][i]
        return "%d  [%s]  %s" % (i + 1, sl["tag"], sl["effect"])

    def mapping_text(self, i):
        mp = self.chain["mappings"][i]
        sl = self.chain["slots"][mp["slot"]] if 0 <= mp["slot"] < len(self.chain["slots"]) else {"tag": "??"}
        knob = "-" if mp.get("knob") is None else ("%d" % (mp["knob"] + 1) if mp["knob"] < self.fx.KNOB_COUNT else "%d (shifted knob %d)" % (mp["knob"] + 1, mp["knob"] + 1 - self.fx.KNOB_COUNT))
        cc = "-" if mp.get("cc") is None else str(mp["cc"])
        return "%s %-14s knob %-18s CC %s" % (sl["tag"], "On" if mp["param"] == self.fx.SLOT_ENABLE else mp["param"], knob, cc)

    def rows(self):
        c = self.chain
        rows = [("name", 0, "Name:             %s" % c["name"]),
                ("primary", 0, "Primary slot:     %s  (owns the LED and the footswitch)" % (self.slot_label(c.get("primary", 0)) if c["slots"] else "-")),
                ("toggle", 0, "Footswitch:       %s" % ("toggles " + ", ".join(self.slot_label(t) for t in c.get("toggle", [])) if c.get("toggle") else "forwarded to the primary slot (tap tempo ...)")),
                ("header", 0, "Effects, in the order the audio goes through them"),]
        rows += [("slot", i, "  " + self.slot_text(i)) for i in range(len(c["slots"]))]
        rows += [("add_slot", 0, "  + add an effect"), ("header", 0, "Knob and MIDI mappings (knobs 1-6; 7-12 are the same knobs after holding the footswitch for a second)")]
        rows += [("map", i, "  " + self.mapping_text(i)) for i in range(len(c["mappings"]))]
        rows += [("add_map", 0, "  + add a mapping")]
        return rows

    def slot_label(self, i):
        return "%d [%s]" % (i + 1, self.chain["slots"][i]["tag"]) if 0 <= i < len(self.chain["slots"]) else "?"

    def current(self):
        rows = self.rows()
        self.cursor = max(0, min(len(rows) - 1, self.cursor))
        return rows[self.cursor]

    def move_cursor(self, delta):
        rows = self.rows()
        pos = self.cursor
        while True:
            pos += delta
            if pos < 0 or pos >= len(rows):
                return
            if rows[pos][0] != "header":
                self.cursor = pos
                return

    def problems(self):
        return self.fx.chain_problems(self.chain, self.sel.enabled, self.sel.chains, self.sel.m, self.sel.mem)

    # -- changes
    def touch(self):
        self.sel.dirty = True

    def set_name(self, name):
        self.chain["name"] = name.strip()
        self.touch()

    def default_tag(self, effect_id):
        used = {sl["tag"] for sl in self.chain["slots"]}
        words = [w for w in effect_id.replace("-", "_").split("_") if w]
        candidates = [("".join(w[0] for w in words[:2]) if len(words) > 1 else effect_id[:2]).title(), effect_id[:2].title(), effect_id[0].upper() + effect_id[-1]]
        for c in candidates:
            if c not in used:
                return c
        for n in range(10, 100):
            if str(n) not in used:
                return str(n)
        return "xx"

    def chainable(self):
        return [e for e in self.sel.effects if self.fx.chain_blocked(e, self.sel.m) is None]

    def add_slot(self, effect_id, tag=None):
        self.chain["slots"].append({"tag": tag or self.default_tag(effect_id), "effect": effect_id})
        self.touch()

    def set_tag(self, i, tag):
        self.chain["slots"][i]["tag"] = tag
        self.touch()

    def set_effect(self, i, effect_id):
        """Another effect in a slot: the mappings to parameters it does not have are dropped."""
        self.chain["slots"][i]["effect"] = effect_id
        params = set(self.fx.effect_params(self.sel.m["by_id"][effect_id])) | {self.fx.SLOT_ENABLE}
        self.chain["mappings"] = [mp for mp in self.chain["mappings"] if mp["slot"] != i or mp["param"] in params]
        self.touch()

    def _renumber(self, mapping):
        """Applies mapping (old slot index -> new index or None) to everything that refers to slots."""
        c = self.chain
        c["mappings"] = [dict(mp, slot=mapping[mp["slot"]]) for mp in c["mappings"] if mapping.get(mp["slot"]) is not None]
        c["toggle"] = sorted(mapping[t] for t in c.get("toggle", []) if mapping.get(t) is not None)
        primary = mapping.get(c.get("primary", 0))
        c["primary"] = primary if primary is not None else 0

    def delete_slot(self, i):
        n = len(self.chain["slots"])
        del self.chain["slots"][i]
        self._renumber({k: (k if k < i else (None if k == i else k - 1)) for k in range(n)})
        self.touch()

    def move_slot(self, i, delta):
        j = i + delta
        n = len(self.chain["slots"])
        if not (0 <= j < n):
            return i
        sl = self.chain["slots"]
        sl[i], sl[j] = sl[j], sl[i]
        swap = {k: k for k in range(n)}
        swap[i], swap[j] = j, i
        self._renumber(swap)
        self.touch()
        return j

    def toggle_footswitch(self, i):
        t = set(self.chain.get("toggle", []))
        t.symmetric_difference_update({i})
        self.chain["toggle"] = sorted(t)
        self.touch()

    def params_of_slot(self, i):
        return [self.fx.SLOT_ENABLE] + self.fx.effect_params(self.sel.m["by_id"][self.chain["slots"][i]["effect"]])

    def next_free_knob(self):
        used = {mp.get("knob") for mp in self.chain["mappings"]}
        return next((k for k in range(self.fx.MAX_KNOB) if k not in used), None)

    def next_free_cc(self):
        used = {mp.get("cc") for mp in self.chain["mappings"]}
        return next((c for c in range(20, 128) if c not in used), None)

    def add_mapping(self, slot, param, knob, cc):
        self.chain["mappings"].append({"slot": slot, "param": param, "knob": knob, "cc": cc})
        self.touch()

    def edit_mapping(self, i, knob, cc):
        self.chain["mappings"][i]["knob"] = knob
        self.chain["mappings"][i]["cc"] = cc
        self.touch()

    def delete_mapping(self, i):
        del self.chain["mappings"][i]
        self.touch()


# ------------------------------------------------------------------ curses

def _init_colors():
    pairs = {}
    if curses.has_colors():
        curses.start_color()
        try:
            curses.use_default_colors()
            bg = -1
        except curses.error:
            bg = curses.COLOR_BLACK
        for n, color in ((1, curses.COLOR_GREEN), (2, curses.COLOR_YELLOW), (3, curses.COLOR_RED), (4, curses.COLOR_CYAN)):
            curses.init_pair(n, color, bg)
            pairs[n] = curses.color_pair(n)
    return pairs


def _put(win, y, x, text, attr=0):
    h, w = win.getmaxyx()
    if 0 <= y < h and x < w:
        try:
            win.addnstr(y, x, text, max(0, w - x - (1 if y == h - 1 else 0)), attr)
        except curses.error:
            pass


def _popup(stdscr, lines, title="", attr_title=0):
    h, w = stdscr.getmaxyx()
    width = min(w - 2, max(len(l) for l in lines + [title]) + 4)
    height = min(h - 2, len(lines) + 2)
    win = curses.newwin(height, width, max(0, (h - height) // 2), max(0, (w - width) // 2))
    win.keypad(True)
    win.box()
    if title:
        _put(win, 0, 2, " %s " % title, attr_title | curses.A_BOLD)
    for i, l in enumerate(lines[:height - 2]):
        _put(win, i + 1, 2, l)
    win.refresh()
    return win.getch()


def _choose(stdscr, title, items):
    """A small list to pick from; returns the item or None."""
    pos = 0
    while True:
        h, w = stdscr.getmaxyx()
        width = min(w - 2, max(len(i) for i in items + [title]) + 6)
        height = min(h - 2, len(items) + 2)
        win = curses.newwin(height, width, max(0, (h - height) // 2), max(0, (w - width) // 2))
        win.keypad(True)
        win.box()
        _put(win, 0, 2, " %s " % title, curses.A_BOLD)
        first = max(0, min(pos - (height - 3), len(items) - (height - 2)))
        for row, item in enumerate(items[first:first + height - 2]):
            _put(win, row + 1, 2, ("> " if first + row == pos else "  ") + item, curses.A_REVERSE if first + row == pos else 0)
        win.refresh()
        k = win.getch()
        if k in (curses.KEY_UP, ord("k")):
            pos = max(0, pos - 1)
        elif k in (curses.KEY_DOWN, ord("j")):
            pos = min(len(items) - 1, pos + 1)
        elif k in (10, 13, curses.KEY_ENTER, ord(" ")):
            return items[pos]
        elif k in (27, ord("q")):
            return None


def draw(stdscr, sel, colors):
    stdscr.erase()
    h, w = stdscr.getmaxyx()
    if h < 6 or w < 40:
        _put(stdscr, 0, 0, "The terminal is too small (needs 40x6).")
        stdscr.refresh()
        return
    _put(stdscr, 0, 0, " Effects in the firmware", curses.A_BOLD)
    cols = " %-20s %6s %6s %7s %7s %6s" % ("effect", "code", "DTCM", "RAM_D2", "SDRAM", "heap")
    _put(stdscr, 1, 0, "      " + cols[1:], curses.A_UNDERLINE)
    rows = h - 5
    if sel.cursor < sel.top:
        sel.top = sel.cursor
    if sel.cursor >= sel.top + rows:
        sel.top = sel.cursor - rows + 1
    for row in range(rows):
        i = sel.top + row
        if i >= len(sel.effects):
            break
        e = sel.effects[i]
        c = sel.effect_cost(e)
        on = e["id"] in sel.enabled
        heap = "%d K" % (c["heap"] // 1024) if c["heap"] is not None else "?"
        sdram = "%.1f M" % (c["sdram"] / 1048576) if c["sdram"] else "-"
        k = lambda v: ("%d K" % (v // 1024)) if v else "-"
        line = " %s %-20s %6s %6s %7s %7s %6s%s" % ("[x]" if on else ("[c]" if sel.in_chain(e["id"]) else "[ ]"), e["id"], k(c["code"]), k(c["dtcm"]), k(c["ram_d2"]), sdram, heap, "  GPL" if e.get("license") == "gpl" else "")
        attr = curses.A_REVERSE if i == sel.cursor else 0
        if on or sel.in_chain(e["id"]):
            attr |= curses.A_BOLD | colors.get(1, 0)
        _put(stdscr, row + 2, 0, line.ljust(w - 1), attr)
    if sel.top > 0:
        _put(stdscr, 1, w - 3, "^")
    if sel.top + rows < len(sel.effects):
        _put(stdscr, h - 4, w - 3, "v")
    info = sel.message or sel.detail(sel.current())
    _put(stdscr, h - 3, 1, info, colors.get(4, 0))
    draw_status(stdscr, sel, colors)
    stdscr.refresh()


def draw_status(stdscr, sel, colors, hint="h: help "):
    h, w = stdscr.getmaxyx()
    text, sev = sel.status()
    bar = (" " + text).ljust(w - 1 - len(hint)) + hint
    attr = curses.A_REVERSE | {0: colors.get(1, 0), 1: colors.get(2, 0), 2: colors.get(3, 0)}[sev]
    _put(stdscr, h - 1, 0, bar.ljust(w - 1), attr)


def _prompt(stdscr, title, initial="", hint=""):
    """A line of text to edit; returns the text, or None if cancelled with Esc."""
    text = initial
    while True:
        h, w = stdscr.getmaxyx()
        width = min(w - 2, max(46, len(title) + 6, len(hint) + 6))
        win = curses.newwin(5, width, max(0, (h - 5) // 2), max(0, (w - width) // 2))
        win.keypad(True)
        win.box()
        _put(win, 0, 2, " %s " % title, curses.A_BOLD)
        _put(win, 2, 2, (text + "_")[-(width - 5):])
        if hint:
            _put(win, 3, 2, hint)
        win.refresh()
        k = win.getch()
        if k in (10, 13, curses.KEY_ENTER):
            return text
        if k == 27:
            return None
        if k in (curses.KEY_BACKSPACE, 127, 8):
            text = text[:-1]
        elif 32 <= k < 127:
            text += chr(k)


def _number(stdscr, title, initial, low, high, blank_ok=True, hint=""):
    """Asks for a whole number (blank = none when blank_ok). Returns (ok, value): ok False if cancelled."""
    text = "" if initial is None else str(initial)
    while True:
        answer = _prompt(stdscr, title, text, hint or ("%d to %d%s" % (low, high, ", empty for none" if blank_ok else "")))
        if answer is None:
            return False, None
        if answer.strip() == "" and blank_ok:
            return True, None
        if answer.strip().lstrip("-").isdigit() and low <= int(answer) <= high:
            return True, int(answer)
        text = answer


def _checklist(stdscr, title, items, checked):
    """Items to switch on and off with Space; Enter when done. Returns the set of checked indexes, or None if cancelled."""
    checked = set(checked)
    pos = 0
    while True:
        h, w = stdscr.getmaxyx()
        width = min(w - 2, max(len(i) for i in items + [title]) + 10)
        height = min(h - 2, len(items) + 3)
        win = curses.newwin(height, width, max(0, (h - height) // 2), max(0, (w - width) // 2))
        win.keypad(True)
        win.box()
        _put(win, 0, 2, " %s " % title, curses.A_BOLD)
        for row, item in enumerate(items[:height - 3]):
            _put(win, row + 1, 2, ("[x] " if row in checked else "[ ] ") + item, curses.A_REVERSE if row == pos else 0)
        _put(win, height - 2, 2, "Space: switch   Enter: done   Esc: cancel")
        win.refresh()
        k = win.getch()
        if k in (curses.KEY_UP, ord("k")):
            pos = max(0, pos - 1)
        elif k in (curses.KEY_DOWN, ord("j")):
            pos = min(len(items) - 1, pos + 1)
        elif k == ord(" "):
            checked.symmetric_difference_update({pos})
        elif k in (10, 13, curses.KEY_ENTER):
            return checked
        elif k == 27:
            return None


CHAIN_HELP = [
    "A chain combines two or more effects into one menu entry: the audio goes through",
    "them in order, all their parameters are in one menu (tagged with the slot's two",
    "character tag, like \"Tr Depth\"), and the six knobs and MIDI CCs are wired as you say.",
    "",
    "  Up / Down     move                   Enter   edit the row under the cursor",
    "  a             add an effect or a mapping (by where the cursor is)",
    "  x             remove the effect or mapping under the cursor",
    "  < / >         move the effect up / down in the chain",
    "  Esc, q        back (the chain is kept; s on the main screen saves everything)",
    "",
    "Primary slot: the effect that owns the LED and the footswitch (tap tempo, ...).",
    "Footswitch toggles: instead of forwarding the footswitch, it switches these effects on",
    "and off. Mappings: knobs 1-6 are the six knobs; 7-12 are the same knobs after holding",
    "the footswitch for a second (the shift bank). An effect that keeps big buffers can only",
    "exist once: in a chain it cannot also be enabled on its own.",
    "",
    "Press any key to close.",
]


def edit_chain(stdscr, sel, chain, colors):
    ed = ChainEditor(sel, chain)
    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        _put(stdscr, 0, 0, " Chain: %s" % (chain["name"] or "(no name)"), curses.A_BOLD)
        rows = ed.rows()
        ed.current()
        height = h - 4
        top = max(0, min(ed.cursor - height // 2, len(rows) - height))
        for r in range(height):
            i = top + r
            if i >= len(rows):
                break
            kind, _, text = rows[i]
            attr = curses.A_BOLD | colors.get(4, 0) if kind == "header" else (curses.A_REVERSE if i == ed.cursor else 0)
            if kind in ("add_slot", "add_map") and i != ed.cursor:
                attr = colors.get(1, 0)
            _put(stdscr, r + 1, 0, text.ljust(w - 1), attr)
        problems = ed.problems()
        _put(stdscr, h - 3, 1, ("! " + problems[0]) if problems else (ed.message or "Enter: edit   a: add   x: remove   < >: move   h: help   Esc: back"), colors.get(3, 0) if problems else colors.get(4, 0))
        draw_status(stdscr, sel, colors, hint="Esc: back ")
        stdscr.refresh()
        key = stdscr.getch()
        kind, idx, _ = ed.current()
        ed.message = ""
        if key in (curses.KEY_UP, ord("k")):
            ed.move_cursor(-1)
        elif key in (curses.KEY_DOWN, ord("j")):
            ed.move_cursor(1)
        elif key in (27, ord("q")):
            return
        elif key in (ord("h"), ord("?")):
            _popup(stdscr, CHAIN_HELP, "Chains")
        elif key in (ord("a"),) or (key in (10, 13, curses.KEY_ENTER) and kind in ("add_slot", "add_map")):
            if kind in ("map", "add_map"):
                _add_mapping(stdscr, ed)
            else:
                _add_slot(stdscr, ed)
        elif key in (ord("x"), curses.KEY_DC):
            if kind == "slot":
                ed.delete_slot(idx)
            elif kind == "map":
                ed.delete_mapping(idx)
        elif key == ord("<") and kind == "slot":
            ed.move_slot(idx, -1)
            ed.cursor = max(0, ed.cursor - 1)
        elif key == ord(">") and kind == "slot":
            ed.move_slot(idx, 1)
            ed.cursor = min(len(ed.rows()) - 1, ed.cursor + (1 if idx + 1 < len(chain["slots"]) else 0))
        elif key in (10, 13, curses.KEY_ENTER):
            _edit_row(stdscr, ed, kind, idx)


def _add_slot(stdscr, ed):
    items = ["%-18s %s" % (e["id"], e.get("note", "")) for e in ed.chainable()]
    pick = _choose(stdscr, "Add an effect to the chain", items)
    if pick is None:
        return
    effect_id = pick.split()[0]
    tag = _prompt(stdscr, "Tag (2 characters, in front of its parameter names)", ed.default_tag(effect_id))
    if tag is not None:
        ed.add_slot(effect_id, tag.strip()[:2])


def _add_mapping(stdscr, ed):
    if not ed.chain["slots"]:
        ed.message = "add effects first"
        return
    slot = 0
    if len(ed.chain["slots"]) > 1:
        pick = _choose(stdscr, "Which effect", ["%s" % ed.slot_text(i) for i in range(len(ed.chain["slots"]))])
        if pick is None:
            return
        slot = int(pick.split()[0]) - 1
    names = ["On  (the effect's own on/off)" if p == ed.fx.SLOT_ENABLE else p for p in ed.params_of_slot(slot)]
    pick = _choose(stdscr, "Which parameter", names)
    if pick is None:
        return
    param = ed.fx.SLOT_ENABLE if pick.startswith("On ") else pick
    free = ed.next_free_knob()
    ok, knob = _number(stdscr, "Knob (1-6; 7-12 after holding the footswitch)", None if free is None else free + 1, 1, ed.fx.MAX_KNOB)
    if not ok:
        return
    free_cc = ed.next_free_cc()
    ok, cc = _number(stdscr, "MIDI CC", free_cc, 0, 127)
    if not ok:
        return
    ed.add_mapping(slot, param, None if knob is None else knob - 1, cc)


def _edit_row(stdscr, ed, kind, idx):
    c = ed.chain
    if kind == "name":
        name = _prompt(stdscr, "Name of the chain (it is drawn on the home screen: about 10 characters fit)", c["name"])
        if name is not None:
            ed.set_name(name)
    elif kind == "primary" and c["slots"]:
        pick = _choose(stdscr, "Primary slot (LED, footswitch)", [ed.slot_text(i) for i in range(len(c["slots"]))])
        if pick:
            c["primary"] = int(pick.split()[0]) - 1
            ed.touch()
    elif kind == "toggle" and c["slots"]:
        chosen = _checklist(stdscr, "The footswitch switches on and off", [ed.slot_text(i) for i in range(len(c["slots"]))], c.get("toggle", []))
        if chosen is not None:
            c["toggle"] = sorted(chosen)
            ed.touch()
    elif kind == "slot":
        pick = _choose(stdscr, "Edit", ["tag: " + c["slots"][idx]["tag"], "effect: " + c["slots"][idx]["effect"]])
        if pick and pick.startswith("tag"):
            tag = _prompt(stdscr, "Tag (2 characters)", c["slots"][idx]["tag"])
            if tag is not None:
                ed.set_tag(idx, tag.strip()[:2])
        elif pick:
            eff = _choose(stdscr, "Effect", ["%-18s %s" % (e["id"], e.get("note", "")) for e in ed.chainable()])
            if eff:
                ed.set_effect(idx, eff.split()[0])
    elif kind == "map":
        mp = c["mappings"][idx]
        ok, knob = _number(stdscr, "Knob (1-12)", None if mp.get("knob") is None else mp["knob"] + 1, 1, ed.fx.MAX_KNOB)
        if not ok:
            return
        ok, cc = _number(stdscr, "MIDI CC", mp.get("cc"), 0, 127)
        if ok:
            ed.edit_mapping(idx, None if knob is None else knob - 1, cc)


def chains_screen(stdscr, sel, colors):
    pos = 0
    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        _put(stdscr, 0, 0, " Effect chains", curses.A_BOLD)
        if not sel.chains:
            _put(stdscr, 2, 2, "No chains yet. Press n to make one.")
        pos = max(0, min(pos, len(sel.chains) - 1))
        for i, c in enumerate(sel.chains[:h - 5]):
            members = " -> ".join("[%s] %s" % (sl["tag"], sl["effect"]) for sl in c["slots"])
            bad = sel.fx.chain_problems(c, sel.enabled, sel.chains, sel.m, sel.mem)
            line = " %-12s %s%s" % (c["name"], members, "   ! " + bad[0] if bad else "")
            _put(stdscr, i + 2, 0, line.ljust(w - 1), curses.A_REVERSE if i == pos else (colors.get(3, 0) if bad else 0))
        _put(stdscr, h - 3, 1, sel.message or "Enter: edit   n: new chain   d: delete   Esc: back", colors.get(4, 0))
        draw_status(stdscr, sel, colors, hint="Esc: back ")
        stdscr.refresh()
        key = stdscr.getch()
        sel.message = ""
        if key in (curses.KEY_UP, ord("k")):
            pos -= 1
        elif key in (curses.KEY_DOWN, ord("j")):
            pos += 1
        elif key in (27, ord("q")):
            return
        elif key == ord("n"):
            name = _prompt(stdscr, "Name of the new chain", "Chain%d" % (len(sel.chains) + 1))
            if name and name.strip():
                sel.chains.append({"name": name.strip(), "slots": [], "mappings": [], "primary": 0, "toggle": []})
                sel.dirty = True
                pos = len(sel.chains) - 1
                edit_chain(stdscr, sel, sel.chains[pos], colors)
        elif key in (10, 13, curses.KEY_ENTER) and sel.chains:
            edit_chain(stdscr, sel, sel.chains[pos], colors)
        elif key in (ord("d"), curses.KEY_DC) and sel.chains:
            answer = _popup(stdscr, ["Delete the chain %s?" % sel.chains[pos]["name"], "", "  y  delete     any other key  keep it"], "Delete")
            if answer in (ord("y"), ord("Y")):
                del sel.chains[pos]
                sel.dirty = True


def ask_save(stdscr):
    return _popup(stdscr, ["Save the changes?", "", "  y  save and quit     n  quit without saving     Esc  keep editing"], "Unsaved changes")


def loop(stdscr, fx):
    curses.curs_set(0)
    stdscr.keypad(True)
    colors = _init_colors()
    sel = Selector(fx)
    while True:
        draw(stdscr, sel, colors)
        key = stdscr.getch()
        h, _ = stdscr.getmaxyx()
        page = max(1, h - 6)
        if key in (curses.KEY_UP, ord("k")):
            sel.move(-1)
        elif key in (curses.KEY_DOWN, ord("j")):
            sel.move(1)
        elif key == curses.KEY_PPAGE:
            sel.move(-1, page)
        elif key == curses.KEY_NPAGE:
            sel.move(1, page)
        elif key == curses.KEY_HOME:
            sel.cursor = 0
        elif key == curses.KEY_END:
            sel.cursor = len(sel.effects) - 1
        elif key == ord(" "):
            cur = sel.current()["id"]
            if cur not in sel.enabled and sel.needs_gpl(cur) and not sel.options.get("ALLOW_GPL"):
                answer = _popup(stdscr, ["%s is GPL-3.0-or-later: a firmware binary that contains it must be" % cur,
                                         "distributed under GPLv3 (or not at all).", "",
                                         "  y  allow the GPL effects and select it     any other key  cancel"], "GPL effect")
                if answer in (ord("y"), ord("Y")):
                    sel.toggle_gpl()
                    sel.toggle()
            else:
                sel.toggle()
        elif key == ord("s"):
            sel.save()
        elif key == ord("a"):
            sel.toggle_gpl()
        elif key == ord("p"):
            name = _choose(stdscr, "Preset", sel.presets())
            if name:
                sel.load_preset(name)
        elif key == ord("c"):
            chains_screen(stdscr, sel, colors)
        elif key == ord("x"):
            what = "%d effects" % len(sel.enabled) + (" and %d chains" % len(sel.chains) if sel.chains else "")
            answer = _popup(stdscr, ["Deselect all %s, and forbid the GPL effects again?" % what, "",
                                     "Nothing is written until you save (s); quitting without saving undoes it.", "",
                                     "  y  clear everything     any other key  cancel"], "Clear everything")
            if answer in (ord("y"), ord("Y")):
                sel.clear()
        elif key == ord("i"):
            _popup(stdscr, sel.info_lines(min(80, stdscr.getmaxyx()[1] - 6)), sel.current()["id"])
        elif key == ord("r"):
            _popup(stdscr, sel.report_lines(), "Memory budget")
        elif key in (ord("h"), ord("?")):
            _popup(stdscr, HELP, "Help")
        elif key == ord("q"):
            if not sel.dirty:
                return
            answer = ask_save(stdscr)
            if answer in (ord("y"), ord("Y")):
                if sel.save():
                    return
            elif answer in (ord("n"), ord("N")):
                return
        elif key == curses.KEY_RESIZE:
            pass


def run(fx):
    os.environ.setdefault("ESCDELAY", "25")
    curses.wrapper(loop, fx)
