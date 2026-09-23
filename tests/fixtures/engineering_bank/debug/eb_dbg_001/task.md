Off-by-one: fill only indices 0..n-1 of a 3-int array; the loop must not write index 3. Fix the loop bound. Compile check uses -fsyntax-only; content check requires 'i < n'.
