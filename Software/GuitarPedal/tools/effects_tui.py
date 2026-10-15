"""The interactive, full-screen effect selection of tools/effects.py (curses; Python's standard library only).

  Up/Down (or k/j), PgUp/PgDn, Home/End   move in the list of effects
  Space                                   select / deselect the effect under the cursor
  s                                       save (writes effects.config and generates the build files)
  p                                       start from a preset
  a                                       allow / forbid the GPL-3.0 effects (ALLOW_GPL)
  r                                       the memory budget in detail
  h                                       help
  q                                       quit (asks about unsaved changes)

The state and what it computes (`Selector`) do not touch the terminal, so it is tested without one (tests/test_effects_tui.py).
"""
import curses
import os

REGION_SHORT = {"dtcm": "DTCM", "ram_d2": "RAM_D2", "sram": "Code", "sdram": "SDRAM"}

HELP = [
    "Choose the effects that are built into the firmware.",
    "",
    "  Up / Down, k / j     move          PgUp / PgDn, Home / End   jump",
    "  Space                select or deselect the effect",
    "  s                    save: writes effects.config and the generated build files",
    "  p                    start from a preset (configs/*.config, everything)",
    "  a                    allow / forbid the GPL-3.0 effects (ALLOW_GPL)",
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
            self.options.update(options)
        self.dirty = True
        self.message = "preset %s loaded (not saved)" % name

    # -- what the selection needs
    def problems(self):
        return self.fx.selection_problems(self.enabled, self.options, self.m)

    def ids(self):
        return [e["id"] for e in self.effects if e["id"] in self.enabled]

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
        text = ("* " if self.dirty else "") + "%d effects  " % len(self.enabled) + text
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
        if e.get("requires"):
            bits.append("needs " + ", ".join(e["requires"]))
        if e.get("conflicts"):
            bits.append("not with " + ", ".join(e["conflicts"]))
        return "; ".join(bits)

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
            self.message = "cannot save: " + "; ".join(problems)
            return False
        self.fx.write_config(self.enabled, self.options, self.m)
        self.fx.generate(quiet=True)
        self.dirty = False
        self.message = "saved: effects.config and the generated build files"
        return True


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
        line = " %s %-20s %6s %6s %7s %7s %6s%s" % ("[x]" if on else "[ ]", e["id"], k(c["code"]), k(c["dtcm"]), k(c["ram_d2"]), sdram, heap, "  GPL" if e.get("license") == "gpl" else "")
        attr = curses.A_REVERSE if i == sel.cursor else 0
        if on:
            attr |= curses.A_BOLD | colors.get(1, 0)
        _put(stdscr, row + 2, 0, line.ljust(w - 1), attr)
    if sel.top > 0:
        _put(stdscr, 1, w - 3, "^")
    if sel.top + rows < len(sel.effects):
        _put(stdscr, h - 4, w - 3, "v")
    info = sel.message or sel.detail(sel.current())
    _put(stdscr, h - 3, 1, info, colors.get(4, 0))
    text, sev = sel.status()
    right = "h: help "
    bar = (" " + text).ljust(w - 1 - len(right)) + right
    attr = curses.A_REVERSE | {0: colors.get(1, 0), 1: colors.get(2, 0), 2: colors.get(3, 0)}[sev]
    _put(stdscr, h - 1, 0, bar.ljust(w - 1), attr)
    stdscr.refresh()


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
            sel.toggle()
        elif key == ord("s"):
            sel.save()
        elif key == ord("a"):
            sel.toggle_gpl()
        elif key == ord("p"):
            name = _choose(stdscr, "Preset", sel.presets())
            if name:
                sel.load_preset(name)
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
