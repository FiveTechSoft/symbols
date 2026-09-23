Off-by-one: the loop must fill only indices 0..n-1. A sentinel at index 3 must remain 12345 (program exits 0 only if the sentinel is untouched and a[0..2]==0,1,2). Fix the loop bound.
