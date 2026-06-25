"""Black-box tests for parsing, exit codes, and the simulator's public CLI."""
import csv
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


class CliTests(unittest.TestCase):
    def run_trace(self, text, *options):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "addresses.trace"
            trace.write_text(text, encoding="ascii")
            return subprocess.run([str(BINARY), "--trace", str(trace), *options],
                                  capture_output=True, text=True, check=False)

    def test_example_csv(self):
        result = self.run_trace("0\n4\n16\n0\n64\n0\n", "--csv")
        self.assertEqual(result.returncode, 0, result.stderr)
        row = list(csv.DictReader(io.StringIO(result.stdout)))[0]
        self.assertEqual((row["accesses"], row["hits"], row["misses"], row["hit_rate_percent"]),
                         ("6", "2", "4", "33.33"))

    def test_hex_comments_leading_zero_and_max_address(self):
        result = self.run_trace("# comment\n\n010 # decimal ten\n0xA\n0Xf\n"
                                "18446744073709551615\n0xffffffffffffffff\n", "--csv")
        self.assertEqual(result.returncode, 0, result.stderr)
        row = list(csv.DictReader(io.StringIO(result.stdout)))[0]
        self.assertEqual((row["hits"], row["misses"]), ("3", "2"))

    def test_empty_trace(self):
        result = self.run_trace("# empty\n", "--csv")
        self.assertEqual(result.returncode, 0, result.stderr)
        row = list(csv.DictReader(io.StringIO(result.stdout)))[0]
        self.assertEqual((row["accesses"], row["hit_rate_percent"]), ("0", "0.00"))

    def test_verbose_mapping(self):
        result = self.run_trace("0\n64\n", "--verbose")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0 0 0 0 0 MISS no", result.stdout)
        self.assertIn("64 4 0 1 0 MISS yes", result.stdout)

    def test_invalid_addresses_report_line(self):
        for bad in ("-1", "+1", "0x", "garbage", "12abc", "1 2", "1.5",
                    "18446744073709551616", "0x10000000000000000"):
            with self.subTest(address=bad):
                result = self.run_trace("# comment\n" + bad + "\n", "--csv")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("line 2:", result.stderr)
                self.assertNotIn("cache_bytes,", result.stdout)

    def test_invalid_sizes(self):
        for size in ("0", "63", "-64", "+64", "64x", "18446744073709551616"):
            with self.subTest(size=size):
                self.assertNotEqual(self.run_trace("0\n", "--cache-size", size).returncode, 0)
        for options in (("--block-size", "128"), ("--block-size", "0"),
                        ("--cache-size", "2097152", "--block-size", "1")):
            self.assertNotEqual(self.run_trace("0\n", *options).returncode, 0)

    def test_invalid_options(self):
        for options in (("--unknown",), ("--cache-size",), ("--csv", "--verbose"),
                        ("--csv", "--csv")):
            self.assertNotEqual(self.run_trace("0\n", *options).returncode, 0)

    def test_missing_trace_and_help(self):
        for options in ([], ["--trace", "does-not-exist.trace"]):
            result = subprocess.run([str(BINARY), *options], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
        result = subprocess.run([str(BINARY), "--help"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage:", result.stdout)


if __name__ == "__main__":
    if BINARY is None or not BINARY.is_file():
        raise SystemExit("Usage: python tests/test_cli.py PATH_TO_CACHE_SIM")
    unittest.main()
