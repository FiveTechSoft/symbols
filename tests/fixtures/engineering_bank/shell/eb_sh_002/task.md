run.sh should exit with status 3 when the argument is 'fail' and 0 otherwise. Currently always exits 0. Fix the script; the checker invokes the shell with run.sh fail and expects exit 3.
