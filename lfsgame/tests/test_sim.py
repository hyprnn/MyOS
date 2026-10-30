"""Simulation tests: disks, fdisk, filesystems, network, users, objectives."""
import json
import unittest

from lfsgame.sim import packages
from lfsgame.sim.game import Game
from lfsgame.sim.machine import Machine
from lfsgame.sim.objectives import OBJECTIVES
from tests.helpers import CH4_4, CH5, CH6, CH7, FULL_WALKTHROUGH, FakeCtx, ch8_walkthrough


class DiskTests(unittest.TestCase):
    def setUp(self):
        self.c = FakeCtx()

    def fdisk(self, keys, dev="/dev/sdb"):
        return self.c.run(f"fdisk {dev}", keys)

    def test_lsblk_sets_flag(self):
        self.c.run("lsblk")
        self.assertIn("sdb", self.c.text())
        self.assertIn("explore", self.c.game.done)

    def test_fdisk_quit_discards(self):
        self.fdisk(["g", "n", "", "", "", "q"])
        self.assertIsNone(self.c.machine.disks["sdb"]["label"])

    def test_fdisk_eof_discards(self):
        self.fdisk(["g", "n"])  # inputs run out => EOF
        self.assertEqual(self.c.machine.disks["sdb"]["parts"], [])

    def test_fdisk_gpt_two_partitions(self):
        self.fdisk(["g", "n", "", "", "+20G", "n", "", "", "", "t", "2", "L", "swap", "p", "w"])
        d = self.c.machine.disks["sdb"]
        self.assertEqual(d["label"], "gpt")
        self.assertEqual([(p["n"], p["type"]) for p in d["parts"]], [(1, "linux"), (2, "swap")])
        self.assertEqual((d["parts"][0]["end"] - d["parts"][0]["start"] + 1) * 512, 20 * 1024 ** 3)
        self.assertEqual(d["parts"][1]["end"], d["sectors"] - 34)
        self.assertIn("Syncing disks", self.c.text())
        self.assertIn("sdb2", self.c.machine.lookup("/dev")["c"])

    def test_fdisk_dos_default_and_bad_values(self):
        self.fdisk(["n", "e", "", "9", "", "", "+10Q", "+12G", "w"])
        t = self.c.text()
        self.assertIn("DOS (MBR)", t)
        self.assertIn("not simulated", t)
        self.assertIn("Value out of range", t)
        d = self.c.machine.disks["sdb"]
        self.assertEqual(d["label"], "dos")
        self.assertEqual(len(d["parts"]), 1)

    def test_fdisk_delete(self):
        self.fdisk(["g", "n", "", "", "+1G", "n", "", "", "+1G", "d", "1", "w"])
        self.assertEqual([p["n"] for p in self.c.machine.disks["sdb"]["parts"]], [2])

    def test_fdisk_host_disk_refused_on_write(self):
        self.fdisk(["d", "2", "w"], dev="/dev/sda")
        self.assertIn("Device or resource busy", self.c.text())
        self.assertEqual(len(self.c.machine.disks["sda"]["parts"]), 2)

    def test_fdisk_on_partition(self):
        self.assertEqual(self.c.run("fdisk /dev/sda1"), 1)
        self.assertIn("is a partition", self.c.text())

    def test_mkfs_refuses_mounted_and_asks_for_existing(self):
        self.assertEqual(self.c.run("mkfs -t ext4 /dev/sda2"), 1)
        self.assertIn("is mounted", self.c.text())
        self.assertEqual(self.c.run("mkfs.ext4 /dev/sdc1", ["n"]), 1)
        self.assertEqual(self.c.machine.fs["/dev/sdc1"]["type"], "vfat")
        self.assertEqual(self.c.run("mkfs.ext4 /dev/sdc1", ["y"]), 0)
        self.assertEqual(self.c.machine.fs["/dev/sdc1"]["type"], "ext4")

    def test_mkfs_whole_disk_wipes_table(self):
        self.c.run("mkfs -t ext4 /dev/sdc", ["y"])
        self.assertEqual(self.c.machine.disks["sdc"]["parts"], [])
        self.assertIn("/dev/sdc", self.c.machine.fs)

    def test_mount_errors_and_busy_umount(self):
        c = self.c
        self.fdisk(["g", "n", "", "", "+12G", "w"])
        self.assertEqual(c.run("mount /dev/sdb1 /mnt/lfs"), 32)
        self.assertIn("mount point does not exist", c.text())
        c.run("mkdir -p /mnt/lfs")
        self.assertEqual(c.run("mount /dev/sdb1 /mnt/lfs"), 32)
        self.assertIn("wrong fs type", c.text())
        c.run("mkfs -t ext4 /dev/sdb1")
        self.assertEqual(c.run("mount -v /dev/sdb1 /mnt/lfs"), 0)
        self.assertIn("lost+found", c.machine.lookup("/mnt/lfs")["c"])
        c.run("touch /mnt/lfs/hello")
        self.assertIn("hello", c.machine.fs["/dev/sdb1"]["root"]["c"])
        self.assertNotIn("hello", c.machine.fs["/dev/sda2"]["root"]["c"]["mnt"]["c"]["lfs"]["c"])
        c.run("cd /mnt/lfs")
        self.assertEqual(c.run("umount /mnt/lfs"), 32)
        self.assertIn("target is busy", c.text())
        c.run("cd /")
        self.assertEqual(c.run("umount /mnt/lfs"), 0)
        self.assertFalse(c.machine.exists("/mnt/lfs/hello"))

    def test_partition_rewrite_drops_filesystem(self):
        c = self.c
        self.fdisk(["g", "n", "", "", "+12G", "w"])
        c.run("mkfs -t ext4 /dev/sdb1")
        self.fdisk(["d", "n", "", "", "+11G", "w"])
        self.assertNotIn("/dev/sdb1", c.machine.fs)

    def test_swap(self):
        c = self.c
        self.fdisk(["g", "n", "", "", "+2G", "w"])
        self.assertEqual(c.run("swapon /dev/sdb1"), 1)
        c.run("mkswap /dev/sdb1")
        self.assertIn("warning", c.text())  # type is not 'Linux swap'
        self.assertEqual(c.run("swapon -v /dev/sdb1"), 0)
        c.run("free -h")
        self.assertIn("2G", c.text())
        c.run("swapoff /dev/sdb1")
        self.assertEqual(c.machine.swaps, [])


class NetUserTests(unittest.TestCase):
    def setUp(self):
        self.c = FakeCtx()

    def test_unknown_host(self):
        self.assertEqual(self.c.run("wget https://example.com/x"), 4)
        self.assertIn("unable to resolve", self.c.text())

    def test_wget_without_continue_makes_copy(self):
        url = packages.MD5SUMS_URL
        self.c.run(f"wget {url}")
        self.c.run(f"wget {url}")
        self.assertTrue(self.c.machine.exists("/root/md5sums.1"))
        self.c.run(f"wget -c {url}")
        self.assertIn("nothing to do", self.c.text())

    def test_glitch_then_continue(self):
        c = self.c
        m = c.machine
        c.run(f"wget {packages.WGET_LIST_URL}")
        c.run("mkdir src")
        self.assertEqual(c.run("wget -q -i wget-list-sysv -c -P src"), 4)
        node = m.lookup(f"/root/src/{m.glitch_file}")
        self.assertTrue(node.get("partial"))
        c.run(f"wget -P src {packages.MD5SUMS_URL}")
        c.run("cd src")
        self.assertEqual(c.run("md5sum -c md5sums"), 1)
        self.assertIn(f"{m.glitch_file}: FAILED", c.text())
        self.assertEqual(c.run("wget -i ../wget-list-sysv -c -P ."), 0)
        self.assertIn("206 Partial Content", c.text())
        self.assertEqual(c.run("md5sum -c md5sums"), 0)
        self.assertTrue(m.has_flag("md5_ok"))

    def test_users_su_passwd(self):
        c = self.c
        c.run("groupadd lfs")
        self.assertEqual(c.run("groupadd lfs"), 1)
        c.run("useradd -s /bin/bash -g lfs -m -k /dev/null lfs")
        self.assertIn("lfs:x:1000:1000::/home/lfs:/bin/bash", c.machine.lookup("/etc/passwd")["data"])
        self.assertEqual(c.run("passwd lfs", ["abcd", "abce"]), 10)
        self.assertEqual(c.run("passwd lfs", ["abcd", "abcd"]), 0)
        c.run("su - lfs")
        self.assertEqual(c.machine.prompt(), "lfs@host:~$ ")
        self.assertEqual(c.run("fdisk /dev/sdb"), 1)
        self.assertEqual(c.run("touch /etc/x"), 1)
        self.assertEqual(c.run("su -", ["wrong"]), 1)
        self.assertEqual(c.run("su - lfs", ["abcd"]), 0)
        c.run("exit")
        c.run("exit")
        self.assertEqual(c.machine.user, "root")
        self.assertFalse(c.menu)


class ObjectiveTests(unittest.TestCase):
    def test_full_walkthrough_completes_everything(self):
        c = FakeCtx(seed=7)
        for line, inputs in FULL_WALKTHROUGH:
            c.run(line, inputs)  # the first wget fails on purpose (network glitch)
        for line, inputs in CH4_4 + CH5 + CH6 + CH7:
            self.assertEqual(c.run(line, inputs), 0, f"{line}\n{c.text()}")
        for line, inputs, must in ch8_walkthrough():
            status = c.run(line, inputs)
            if must:
                self.assertEqual(status, 0, f"{line}\n{c.text()}")
        missing = [o.id for o in OBJECTIVES if o.id not in c.game.done]
        self.assertEqual(missing, [])
        self.assertTrue(c.game.finished())

    def test_wrong_disk_does_not_count(self):
        c = FakeCtx()
        c.run("fdisk /dev/sdc", ["d", "n", "", "", "", "", "w"])
        c.run("mkfs -t ext4 /dev/sdc1", ["y"])
        c.run("export LFS=/mnt/lfs; mkdir -p $LFS; mount /dev/sdc1 $LFS")
        self.assertNotIn("partition", c.game.done)
        self.assertNotIn("mount", c.game.done)
        self.assertIn("env", c.game.done)

    def test_too_small_partition(self):
        c = FakeCtx()
        c.run("fdisk /dev/sdb", ["g", "n", "", "", "+5G", "w"])
        self.assertNotIn("partition", c.game.done)

    def test_sticky_completion(self):
        c = FakeCtx()
        c.run("export LFS=/mnt/lfs")
        c.run("unset LFS")
        self.assertIn("env", c.game.done)

    def test_game_roundtrip(self):
        c = FakeCtx()
        for line, inputs in FULL_WALKTHROUGH[:10]:
            c.run(line, inputs)
        data = json.loads(json.dumps(c.game.to_dict()))
        g = Game.from_dict(data)
        self.assertEqual(g.done, c.game.done)
        self.assertTrue(g.machine.exists("/mnt/lfs/sources"))
        self.assertEqual(g.machine.prompt(), c.machine.prompt())

    def test_machine_from_dict_validates(self):
        good = Machine.new(1).to_dict()
        for key, bad in (("mounts", {}), ("cwd", 5), ("user", "nobody"), ("fs", {})):
            d = json.loads(json.dumps(good))
            d[key] = bad
            with self.assertRaises((ValueError, TypeError), msg=key):
                Machine.from_dict(d)


if __name__ == "__main__":
    unittest.main()
