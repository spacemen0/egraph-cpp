import os
import sys
import argparse
import numpy as np

def generate_matrix(rows: int, cols: int, prop_type: str = "general") -> np.ndarray:
    """
    Generates a matrix of shape (rows, cols) with specific mathematical properties.
    Supported types:
      - general / default: Standard uniform random matrix in [0, 1)
      - fullrank / full_rank: Guaranteed full-rank matrix via QR/SVD construction
      - symmetric: Symmetric matrix (0.5 * (A + A^T))
      - spd / positive_definite: Symmetric Positive-Definite matrix (A A^T + n*I)
      - lower / lower_triangular: Lower triangular matrix (tril)
      - upper / upper_triangular: Upper triangular matrix (triu)
    """
    prop = prop_type.lower().replace("-", "_")

    if prop in ["symmetric", "sym"]:
        if rows != cols:
            raise ValueError(f"Symmetric matrix must be square, got {rows}x{cols}")
        A = np.random.randn(rows, cols)
        return 0.5 * (A + A.T)

    elif prop in ["positive_definite", "pos_def", "spd", "posdef"]:
        if rows != cols:
            raise ValueError(f"Positive-definite matrix must be square, got {rows}x{cols}")
        A = np.random.randn(rows, cols)
        return A @ A.T + rows * np.eye(rows)

    elif prop in ["full_rank", "fullrank"]:
        k = min(rows, cols)
        U, _ = np.linalg.qr(np.random.randn(rows, k), mode="reduced")
        vt, _ = np.linalg.qr(np.random.randn(cols, k), mode="reduced")
        singular_values = np.linspace(1.0, 10.0, k)
        return U @ np.diag(singular_values) @ vt.T

    elif prop in ["lower_triangular", "lower", "tril"]:
        A = np.random.randn(rows, cols)
        L = np.tril(A)
        if rows == cols:
            np.fill_diagonal(L, np.abs(np.diag(L)) + 1.0)
        return L

    elif prop in ["upper_triangular", "upper", "triu"]:
        A = np.random.randn(rows, cols)
        U = np.triu(A)
        if rows == cols:
            np.fill_diagonal(U, np.abs(np.diag(U)) + 1.0)
        return U

    elif prop in ["identity", "eye"]:
        if rows != cols:
            raise ValueError(f"Identity matrix must be square, got {rows}x{cols}")
        return np.eye(rows)

    elif prop in ["general", "default", "rand"]:
        return np.random.rand(rows, cols)

    else:
        raise ValueError(
            f"Unknown matrix property: '{prop_type}'. "
            "Supported types: general, fullrank, symmetric, spd (positive_definite), lower, upper, identity."
        )

def main():
    parser = argparse.ArgumentParser(
        description="Generate random matrices with specific properties (fullrank, symmetric, spd, lower/upper triangular)."
    )
    parser.add_argument("rows", type=int, help="Number of rows")
    parser.add_argument("cols", type=int, help="Number of columns")
    parser.add_argument("filename", type=str, help="Output filename (saved in examples/data)")
    parser.add_argument(
        "--type", "-t",
        type=str,
        default="general",
        choices=["general", "fullrank", "symmetric", "spd", "positive_definite", "lower", "upper", "identity"],
        help="Matrix property type: general, fullrank, symmetric, spd, lower, upper, identity (default: general)"
    )
    parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility")
    parser.add_argument("--fmt", type=str, default="%.6f", help="Output format specifier (default: %%.6f)")
    parser.add_argument("--outdir", type=str, default=None, help="Custom output directory (default: examples/data)")

    args = parser.parse_args()

    if args.seed is not None:
        np.random.seed(args.seed)

    try:
        matrix_data = generate_matrix(args.rows, args.cols, args.type)
    except ValueError as err:
        print(f"Error: {err}", file=sys.stderr)
        sys.exit(1)

    output_dir = args.outdir
    if output_dir is None:
        output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "examples", "data"))
    os.makedirs(output_dir, exist_ok=True)

    filename = args.filename if (args.filename.endswith(".csv") or args.filename.endswith(".txt")) else args.filename + ".csv"
    file_path = os.path.join(output_dir, filename)

    np.savetxt(file_path, matrix_data, delimiter=",", fmt=args.fmt)
    print(f"[{args.type.upper()}] Matrix ({args.rows}x{args.cols}) successfully saved to {file_path}")

if __name__ == "__main__":
    main()
