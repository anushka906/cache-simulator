"""Run deterministic synthetic workloads; CSV results come from the C++ binary."""
import argparse
import csv
import io
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="Path to the compiled cache_sim executable")
    parser.add_argument("--output", type=Path, default=Path("results/cache_analysis.csv"))
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error("simulator executable does not exist; build it first")
    workloads = {
        "sequential": list(range(0, 256, 4)),
        "repeated_working_set": list(range(0, 64, 4)) * 4,
        "conflicting_pair": [0, 64] * 32,
        "stride_64": list(range(0, 1024, 64)) * 4,
    }
    rows = []
    with tempfile.TemporaryDirectory() as directory:
        for name, addresses in workloads.items():
            trace = Path(directory) / (name + ".trace")
            trace.write_text("".join(str(address) + "\n" for address in addresses), encoding="ascii")
            for capacity in (64, 128, 256):
                for block in (8, 16, 32):
                    result = subprocess.run([str(binary), "--trace", str(trace),
                                             "--cache-size", str(capacity),
                                             "--block-size", str(block), "--csv"],
                                            check=True, capture_output=True, text=True)
                    summaries = list(csv.DictReader(io.StringIO(result.stdout)))
                    if len(summaries) != 1:
                        raise RuntimeError("Expected exactly one simulator summary")
                    rows.append({"workload": name, **summaries[0]})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} measured configurations to {args.output}")
    print("Each workload/configuration starts with an empty cache. No timing is modelled.")


if __name__ == "__main__":
    main()
