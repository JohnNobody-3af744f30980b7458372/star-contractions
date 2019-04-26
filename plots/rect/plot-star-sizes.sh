#!/bin/bash

# !!!! requrires gnuplot >= 5 !!!!

XX="`tempfile`"
trap 'rm -f "$XX"' EXIT

plot() {
  out=$1
  shift
  cat "$@" |
  sed -nre 's/.*Star with ([0-9]+) terminals.*/\1/p' > "$XX"

  gnuplot /dev/stdin <<EOF
    set terminal svg enhanced background rgb 'white';
    set datafile separator ',';
    set output '$out';
 
    set boxwidth 0.9
    set style fill solid 0.5
    set logscale y
    set xrange [0:140]
    set yrange [0:1000*1000]
 
    plot "$XX" using (\$1):(1.0) smooth freq w boxes lc rgb "green" notitle
EOF
}

plot star-sizes-noprec.svg logs/instance*.noprec.nozel.log
plot star-sizes-prec.svg logs/instance*.prec.nozel.log


