"""Headless tests for lfsgame.save (no tkinter)."""
import json
import os
import tempfile
import unittest
from pathlib import Path

from lfsgame import save
from tests.helpers import FakeCtx


class SaveTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self._old = os.environ.get("LFSGAME_SAVE_DIR")
        os.environ["LFSGAME_SAVE_DIR"] = self.tmp.name
        self.addCleanup(self._restore)

    def _restore(self):
        if self._old is None:
            os.environ.pop("LFSGAME_SAVE_DIR", None)
        else:
            os.environ["LFSGAME_SAVE_DIR"] = self._old

    def test_status_none(self):
        self.assertEqual(save.status(), ("none", ""))

    def test_roundtrip_unicode(self):
        runs = [("line1\n", None), ("строка2\n", "err")]
        save.write_save(save.GameState(history=["echo привет", "ls"], scrollback=runs))
        st = save.read_save()
        self.assertEqual(st.history, ["echo привет", "ls"])
        self.assertEqual(st.scrollback, runs)
        self.assertIsNone(st.game)
        self.assertRegex(st.saved_at, r"^\d{4}-\d\d-\d\d \d\d:\d\d$")
        self.assertEqual(save.status()[0], "ok")

    def test_bak_created_on_second_save(self):
        save.write_save(save.GameState(history=["a"]))
        save.write_save(save.GameState(history=["b"]))
        bak = save.save_path().with_name("save.json.bak")
        self.assertTrue(bak.exists())
        self.assertEqual(json.loads(bak.read_text(encoding="utf-8"))["history"], ["a"])

    def test_no_tmp_left_behind(self):
        save.write_save(save.GameState())
        self.assertEqual([p.name for p in Path(self.tmp.name).glob(".save-*")], [])

    def test_corrupt_json(self):
        save.save_path().write_text("{not json", encoding="utf-8")
        self.assertEqual(save.status()[0], "corrupt")
        with self.assertRaises(save.SaveError):
            save.read_save()

    def test_wrong_shapes(self):
        for bad in ('[]', '{"version": "1"}', '{"version": 1, "history": [1]}',
                    '{"version": 1, "scrollback": 5}', '{"version": true}',
                    '{"version": 2, "scrollback": [["a"]]}', '{"version": 2, "game": {}}',
                    '{"version": 2, "game": {"machine": {"cwd": 1}}}'):
            save.save_path().write_text(bad, encoding="utf-8")
            with self.assertRaises(save.SaveError, msg=bad):
                save.read_save()

    def test_newer_version_rejected(self):
        save.save_path().write_text(json.dumps({"version": save.SAVE_VERSION + 1}), encoding="utf-8")
        with self.assertRaises(save.SaveError):
            save.read_save()

    def test_trimming(self):
        lines = "\n".join(str(i) for i in range(save.MAX_SCROLLBACK_LINES + 50))
        big = save.GameState(history=[str(i) for i in range(save.MAX_HISTORY + 50)],
                             scrollback=[(lines[:100], "dim"), (lines[100:], None)])
        save.write_save(big)
        st = save.read_save()
        self.assertEqual(len(st.history), save.MAX_HISTORY)
        self.assertEqual(st.history[-1], str(save.MAX_HISTORY + 49))
        self.assertEqual(len(st.scrollback_text().split("\n")), save.MAX_SCROLLBACK_LINES)
        self.assertTrue(st.scrollback_text().endswith(str(save.MAX_SCROLLBACK_LINES + 49)))

    def test_trim_runs_keeps_tags(self):
        runs = [("a\nb\n", "err"), ("c\n", None), ("d\ne", "ok")]
        self.assertEqual(save.trim_runs(runs, 3), [("c\n", None), ("d\ne", "ok")])
        self.assertEqual(save.trim_runs(runs, 4), [("b\n", "err"), ("c\n", None), ("d\ne", "ok")])

    def test_v1_migration(self):
        save.save_path().write_text(json.dumps(
            {"version": 1, "saved_at": "2026-10-30 10:00", "history": ["ls"],
             "scrollback": "old text\n"}), encoding="utf-8")
        st = save.read_save()
        self.assertEqual(st.scrollback, [("old text\n", None)])
        self.assertIsNone(st.game)
        self.assertEqual(save.status(), ("ok", "2026-10-30 10:00"))

    def test_game_roundtrip(self):
        c = FakeCtx()
        c.run("export LFS=/mnt/lfs; mkdir -p $LFS; lsblk")
        save.write_save(save.GameState(game=c.game))
        st = save.read_save()
        self.assertEqual(st.game.machine.env["LFS"], "/mnt/lfs")
        self.assertTrue(st.game.machine.isdir("/mnt/lfs"))
        self.assertEqual(st.game.done, c.game.done)

    def test_save_dir_env_and_xdg(self):
        self.assertEqual(save.save_dir(), Path(self.tmp.name))
        os.environ.pop("LFSGAME_SAVE_DIR")
        os.environ["XDG_DATA_HOME"] = "/x/y"
        try:
            self.assertEqual(save.save_dir(), Path("/x/y/lfsgame"))
        finally:
            os.environ.pop("XDG_DATA_HOME")
            os.environ["LFSGAME_SAVE_DIR"] = self.tmp.name


if __name__ == "__main__":
    unittest.main()
