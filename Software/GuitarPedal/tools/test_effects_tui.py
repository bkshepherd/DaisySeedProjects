#!/usr/bin/env python3
"""Tests of the interactive effect selection (tools/effects_tui.py): the state without a terminal, and the real curses screen driven through a pseudo terminal.
Run: python3 tools/test_effects_tui.py (nothing in the repository is changed: the configuration and the generated files go to a temporary directory)."""
import fcntl
import os
import pty
import re
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import effects as fx  # noqa: E402
import effects_tui as tui  # noqa: E402


class SelectorTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.saved = (fx.CONFIG, fx.GEN)
        fx.CONFIG = os.path.join(self.tmp.name, "effects.config")
        fx.GEN = os.path.join(self.tmp.name, "generated")
        open(fx.CONFIG, "w").write("ALLOW_GPL=n\nuranus=y\n")
        self.sel = tui.Selector(fx)

    def tearDown(self):
        fx.CONFIG, fx.GEN = self.saved
        self.tmp.cleanup()

    def index(self, effect_id):
        return [e["id"] for e in self.sel.effects].index(effect_id)

    def test_starts_from_the_config(self):
        self.assertEqual(self.sel.enabled, {"uranus"})
        self.assertFalse(self.sel.dirty)
        text, sev = self.sel.status()
        self.assertIn("1 effects", text)
        self.assertEqual(sev, 0)

    def test_space_selects_and_deselects(self):
        self.sel.cursor = self.index("venus")
        self.sel.toggle()
        self.assertIn("venus", self.sel.enabled)
        self.assertTrue(self.sel.dirty)
        self.assertTrue(self.sel.status()[0].startswith("* "))
        self.sel.toggle()
        self.assertNotIn("venus", self.sel.enabled)

    def test_cursor_stays_in_the_list(self):
        self.assertEqual(self.sel.cursor, self.index("uranus"))  # starts on the first selected effect
        self.sel.move(-1, 10000)
        self.assertEqual(self.sel.cursor, 0)
        self.sel.move(1, 10000)
        self.assertEqual(self.sel.cursor, len(self.sel.effects) - 1)

    def test_the_status_follows_the_selection(self):
        before = self.sel.usage()[0]
        self.sel.toggle(next(e["id"] for e in self.sel.effects if self.sel.effect_cost(e)["sdram"] > 1 << 20))
        after = self.sel.usage()[0]
        self.assertGreater(after["sdram"], before["sdram"])

    def test_a_selection_that_does_not_fit_is_flagged(self):
        self.sel.load_preset("everything")
        text, sev = self.sel.status()
        self.assertEqual(sev, 2, text)
        self.assertTrue("does NOT fit" in text or "cannot be built" in text)

    def test_gpl_needs_allow_gpl_and_cannot_be_saved_without(self):
        self.sel.toggle("earth")
        self.assertTrue(self.sel.problems())
        self.assertEqual(self.sel.status()[1], 2)
        self.assertFalse(self.sel.save())
        self.assertIn("cannot save", self.sel.message)
        self.assertEqual(open(fx.CONFIG).read(), "ALLOW_GPL=n\nuranus=y\n")  # untouched
        self.sel.toggle_gpl()
        self.assertFalse([p for p in self.sel.problems() if "GPL" in p])

    def test_save_writes_the_config_and_the_build_files(self):
        self.sel.toggle("venus")
        self.assertTrue(self.sel.save())
        self.assertFalse(self.sel.dirty)
        enabled, _ = fx.read_config()
        self.assertEqual(enabled, {"uranus", "venus"})
        mk = open(os.path.join(fx.GEN, "effects.mk")).read()
        self.assertIn("venus_module.cpp", mk)
        self.assertIn("new VenusModule()", open(os.path.join(fx.GEN, "effects_instances.inc")).read())

    def test_presets(self):
        self.assertIn("everything", self.sel.presets())
        self.assertIn("uranus-only", self.sel.presets())
        self.sel.load_preset("uranus-only")
        self.assertEqual(self.sel.enabled, {"uranus"})
        self.assertTrue(self.sel.dirty)

    def test_report_and_detail(self):
        self.assertTrue(any("verdict" in l for l in self.sel.report_lines()))
        gpl = next(e for e in self.sel.effects if e.get("license") == "gpl")
        self.assertIn("GPL", self.sel.detail(gpl))


class ScreenTest(unittest.TestCase):
    """The real screen, in a pseudo terminal of 24 x 80."""

    def run_keys(self, keys, rows=24, cols=80):
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        env = dict(os.environ, TERM="xterm-256color", ESCDELAY="25")
        proc = subprocess.Popen([sys.executable, os.path.join(HERE, "effects.py"), "tui"], stdin=slave, stdout=slave, stderr=slave, env=env, close_fds=True)
        os.close(slave)
        out = b""

        def drain(wait):
            nonlocal out
            end = time.time() + wait
            while time.time() < end:
                r, _, _ = select.select([master], [], [], 0.05)
                if r:
                    try:
                        out += os.read(master, 65536)
                    except OSError:
                        return

        drain(0.8)
        for k in keys:
            os.write(master, k)
            drain(0.25)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        drain(0.2)
        os.close(master)
        text = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][A-Z0-9]|\x1b[=>]", b" ", out).decode("utf-8", "replace")
        return text, proc.returncode

    def test_screen_shows_the_list_the_status_bar_and_the_help_hint(self):
        text, code = self.run_keys([b"q"])
        self.assertEqual(code, 0)
        self.assertIn("uranus", text)  # the cursor starts on the first selected effect, so the list is scrolled to show it
        self.assertIn("venus", text)
        self.assertIn("DTCM", text)
        self.assertIn("SDRAM", text)
        self.assertIn("h: help", text)

    def test_arrows_space_help_and_quit_without_saving(self):
        before = open(fx.CONFIG).read()
        text, code = self.run_keys([b"\x1b[B", b"\x1b[B", b" ", b"h", b"x", b"q", b"n"])
        self.assertEqual(code, 0)
        self.assertIn("Help", text)
        self.assertIn("Unsaved changes", text)  # the selection was changed, so quitting asked
        self.assertIn("* ", text)
        self.assertEqual(open(fx.CONFIG).read(), before)  # n: nothing saved

    def test_a_small_terminal_does_not_crash(self):
        text, code = self.run_keys([b"q"], rows=5, cols=30)
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
