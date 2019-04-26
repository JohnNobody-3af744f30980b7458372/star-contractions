#!/bin/bash

plot-all() {(
  cd "$1"
  for col in mst mstp zel zelm zelp; do
    for prec in prec noprec; do
      if [[ prec == $prec && $col == zel* ]]; then
        continue
      fi
      YRANGE="$2" COL=$col PREC=$prec ./plot-boxes.sh
    done
  done
)}

plot-all rect [97:120]
plot-all pace

cd pace-work
for col in W1 W2; do
  for prec in prec noprec; do
    if [[ $col == W1 ]]; then
      export YRANGE=[0.0001:2] \
             YLABEL="Recalculated stars (fraction)"
    else
      export YRANGE=[0.0000001:10] \
             YLABEL="Visited vertices (fraction of N^2)"
    fi
    OPTS="set logscale y" COL=$col PREC=$prec ./plot-boxes.sh
  done
done


