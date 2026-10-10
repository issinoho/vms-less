#!/usr/bin/env bash
# ttytest.sh <node> - run tools/ttytest.py (interactive checks) with the python
# that has pexpect and pyte: $TTYTEST_PYTHON, else python3.
exec "${TTYTEST_PYTHON:-python3}" "$(dirname "$0")/ttytest.py" "$@"
