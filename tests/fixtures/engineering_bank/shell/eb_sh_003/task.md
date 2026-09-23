run.sh uses an unquoted variable so a path with spaces/multiple spaces is collapsed. The checker runs `sh run.sh 'a  b'` (two spaces) and requires the stdout line to be exactly `a  b`.
