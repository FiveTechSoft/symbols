Make the pointer parameter of scale const-correct: the function must not modify *p, and the signature should use const int *. Update the call site if needed. Signature must contain 'const int *'.
