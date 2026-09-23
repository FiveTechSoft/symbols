helper_sum is used before it is declared, which fails with -Werror=implicit-function-declaration. Provide a prototype (or definition before use) so the file compiles.
