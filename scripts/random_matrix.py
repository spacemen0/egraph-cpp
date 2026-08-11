import os
import sys
import numpy as np

# 1. Read CLI parameters: row, col, filename
if len(sys.argv) != 4:
    print("Usage: python random_matrix.py <rows> <cols> <filename>")
    sys.exit(1)

rows = int(sys.argv[1])
cols = int(sys.argv[2])
filename = sys.argv[3]

# 2. Generate random matrix data (floating-point numbers)
matrix_data = np.random.rand(rows, cols)

# 3. Save to examples/data
output_dir = os.path.join(os.path.dirname(__file__), "..", "examples", "data")
os.makedirs(output_dir, exist_ok=True)

if not filename.endswith(".txt"):
    filename += ".txt"

file_path = os.path.join(output_dir, filename)
np.savetxt(file_path, matrix_data, delimiter=" ", fmt="%.6f")

print(f"Matrix successfully saved to {file_path}")
