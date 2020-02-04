#!/bin/bash

for prec in prec noprec; do
  for col in mst mstp zel zelm zelp; do
    echo "> $col $prec"
    COL=$col PREC=$prec ./plot-boxes.sh
  done
done

