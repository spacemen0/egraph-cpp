#!/usr/bin/env python3
import argparse
import pathlib
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Configure, build, and run egraph_cli")
    parser.add_argument(
        "mode",
        nargs="?",
        default="0",
        choices=["0", "1"],
        help="0 uses input.txt, 1 uses input_symbolic.txt",
    )
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent
    input_file = "input.txt" if args.mode == "0" else "input_symbolic.txt"
    input_path = repo_root / input_file

    if not input_path.is_file():
        print(f"Error: input file not found: {input_file}", file=sys.stderr)
        return 1

    print("Configuring project...")
    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            "build",
            "-DENABLE_COVERAGE=OFF",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        cwd=repo_root,
    )

    print("Building CLI...")
    run(["cmake", "--build", "build", "--target", "egraph_cli"], cwd=repo_root)

    print(f"Running CLI with {input_file}...")
    cli_path = repo_root / "build" / "src" / "egraph_cli"
    with input_path.open("rb") as stdin_stream:
        subprocess.run([str(cli_path)], cwd=repo_root, check=True, stdin=stdin_stream)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
