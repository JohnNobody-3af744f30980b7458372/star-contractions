#!/bin/bash

dsc=`git describe --always --dirty`
msg="`git show --format=%s -s | sed -re 's/"/\\\\"/g'`"

if grep -qe '-dirty$' <<<"$dsc" && (( ! DIRTY )); then
  echo "Repo is DIRTY, aborting!" >&2
  exit 1
fi

printf "%q" -DVERSION="\"$dsc $msg\""

