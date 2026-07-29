import numpy as np
import subprocess
import os

def test_batch():
    num_tests = 100
    
    inputs = []
    expected_xs = []
    
    for _ in range(num_tests):
        # Random dimension A >= B
        B = np.random.randint(1, 20)
        A = np.random.randint(B, 40)
        
        # Randomize matrix M (A x B) and vector x (B)
        M = np.random.randn(A, B)
        x = np.random.randn(B)
        
        # Compute n
        n = M @ x
        
        # Expects M in column-major order 
        M_flat = M.flatten(order='F')
        
        # Convert to strings for stdin
        inputs.append(f"{A} {B}")
        inputs.append(" ".join(map(str, M_flat)))
        inputs.append(" ".join(map(str, n)))
        expected_xs.append(x)
        
    input_str = "\n".join(inputs) + "\n"
    
    executable = "./build/src/example"
    if not os.path.exists(executable):
        print(f"Error: Executable {executable} not found. Did you build it?")
        return
        
    print(f"Running {num_tests} batch tests against {executable}...")
    
    process = subprocess.Popen(
        [executable],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    stdout, stderr = process.communicate(input=input_str)
    
    if process.returncode != 0:
        print(f"Executable failed with return code {process.returncode}")
        print(f"Stderr: {stderr}")
        return
        
    output_lines = stdout.strip().split("\n")
    
    results = []
    for line in output_lines:
        try:
            vals = [float(v) for v in line.split()]
            if len(vals) > 0:
                results.append(np.array(vals))
        except ValueError:
            # Skip lines that are not valid numbers (logs from the executable etc)
            pass
            
    if len(results) != num_tests:
        print(f"Error: expected {num_tests} result lines, but got {len(results)}")
        print("Raw stdout:")
        print(stdout)
        return
        
    # Test if it is near the x we randomized
    all_passed = True
    max_error = 0.0
    
    for i in range(num_tests):
        x_hat = results[i]
        x_true = expected_xs[i]
        
        # Check size
        if len(x_hat) != len(x_true):
            print(f"Test {i} failed: expected size {len(x_true)}, got {len(x_hat)}")
            all_passed = False
            continue
            
        # Check closeness
        error = np.max(np.abs(x_hat - x_true))
        max_error = max(max_error, error)
        
        if not np.allclose(x_hat, x_true, atol=1e-4, rtol=1e-4):
            print(f"Test {i} failed with error {error}:")
            print(f"  Expected: {x_true}")
            print(f"  Got:      {x_hat}")
            all_passed = False
            
    if all_passed:
        print(f"All {num_tests} tests passed successfully! Maximum absolute error: {max_error:.2e}")
        print(results[0])  # Print the first result for reference
        print(expected_xs[0])
    else:
        print(f"\033[91mSome tests failed!\033[0m")

if __name__ == "__main__":
    test_batch()
