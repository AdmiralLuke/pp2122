#/bin/bash
message="$(hostname --short) : $(date --iso-8601=ns)"
echo "$message"
