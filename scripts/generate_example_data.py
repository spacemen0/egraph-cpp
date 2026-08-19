import os
import sys
import argparse
import numpy as np

# Import generate_matrix from random_matrix module
sys.path.insert(0, os.path.dirname(__file__))
from random_matrix import generate_matrix

def generate_all_example_matrices(output_dir: str, seed: int = 42, fmt: str = "%.6f"):
    """
    Generates all sample datasets required by the project examples:
      - OLS: ols_x.csv (2000x500 tall), ols_y.csv (2000x1)
      - GLS: gls_m.csv (3000x3000 SPD), gls_x.csv (3000x500 tall), gls_y.csv (3000x1)
      - ImageRestoration: image_h.csv (1000x3000 wide), image_y.csv (1000x1), image_x.csv (3000x1)
    """
    if seed is not None:
        np.random.seed(seed)

    os.makedirs(output_dir, exist_ok=True)

    datasets = [
        # OLS Example Datasets
        ("ols_x.csv", 4000, 1000, "fullrank", "OLS Tall Matrix X (4000x1000)"),
        ("ols_y.csv", 4000, 1, "general", "OLS Column Vector y (4000x1)"),

        # GLS Example Datasets
        ("gls_m.csv", 4000, 4000, "spd", "GLS Symmetric Positive-Definite Covariance Matrix M (4000x4000)"),
        ("gls_x.csv", 4000, 1000, "fullrank", "GLS Tall Design Matrix X (4000x1000)"),
        ("gls_y.csv", 4000, 1, "general", "GLS Column Vector y (4000x1)"),

        # Image Restoration Example Datasets
        ("image_h.csv", 2000, 6000, "fullrank", "Image Restoration Wide Matrix H (2000x6000)"),
        ("image_y.csv", 2000, 1, "general", "Image Restoration Column Vector y (2000x1)"),
        ("image_x.csv", 6000, 1, "general", "Image Restoration Column Vector x (6000x1)"),

        # Stochastic Newton Example Datasets
        ("stochastic_newton_b.csv", 2000, 2000, "spd", "StoNewton SPD Matrix B (2000x2000)"),
        ("stochastic_newton_in.csv", 2000, 2000, "identity", "StoNewton Identity Matrix In (2000x2000)"),
        ("stochastic_newton_a.csv", 4000, 2000, "fullrank", "StoNewton Tall Matrix A (4000x2000)"),
        ("stochastic_newton_w.csv", 4000, 500, "fullrank", "StoNewton Tall Matrix W_k (4000x500)"),
        ("stochastic_newton_il.csv", 500, 500, "identity", "StoNewton Identity Matrix Il (500x500)"),
    ]

    print(f"Generating example matrices in directory: {output_dir}\n" + "=" * 60)

    for filename, rows, cols, prop_type, description in datasets:
        file_path = os.path.join(output_dir, filename)
        matrix = generate_matrix(rows, cols, prop_type)
        np.savetxt(file_path, matrix, delimiter=",", fmt=fmt)
        print(f"  ✓ Saved {filename:<18} ({rows}x{cols}, type: {prop_type:<8}) - {description}")

    print("=" * 60 + "\nAll example datasets successfully generated.")

def main():
    parser = argparse.ArgumentParser(
        description="Generate all CSV matrix datasets needed by C++ examples (OLS, GLS, ImageRestoration)."
    )
    parser.add_argument(
        "--outdir", "-o",
        type=str,
        default=None,
        help="Target output directory (default: examples/data)"
    )
    parser.add_argument("--seed", "-s", type=int, default=42, help="Random seed for reproducibility (default: 42)")
    parser.add_argument("--fmt", type=str, default="%.6f", help="Output floating-point format specifier (default: %%.6f)")

    args = parser.parse_args()

    outdir = args.outdir
    if outdir is None:
        outdir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "examples", "data"))

    generate_all_example_matrices(outdir, seed=args.seed, fmt=args.fmt)

if __name__ == "__main__":
    main()
