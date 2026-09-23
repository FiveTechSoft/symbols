run.sh uses an unquoted variable so a path with spaces breaks. Quote "$1" when echoing it. Checker requires the literal line echo "$1" and forbids unquoted echo $1.
