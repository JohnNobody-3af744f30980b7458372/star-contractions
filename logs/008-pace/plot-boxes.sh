#!/bin/bash

XX="`tempfile`"
trap 'rm -f "$XX"' EXIT

: ${COL:=mst}
: ${PREC:=noprec}

if grep -q "zel" <<<"$COL"; then
  ZEL=zel
else
  ZEL=nozel
fi

files=( "$@" )
if (( ${#files[@]} == 0 )); then
  for f in logs/*.$PREC.$ZEL.log; do
    if grep -q FINAL\ WEIGHT < $f; then
      files+=( $f )
    fi
  done
fi

Rscript --vanilla ../process.R "$COL" mst_box "${files[@]}" > "$XX"

declare -A T

T=(
  [prec]=" (Improved stars)"
  [noprec]=""

  [zel]=Zelikovsky
  [zelm]=Zelikovsky-
  [zelp]=Zelikovsky+

  [mst]=MST
  [mstp]=MST+

  [W1]="Work: recalculated ratios"
  [W2]="Work: visited vertices"
)

if [[ "$YRANGE" == auto ]]; then
  YRANGE=""
else
  YRANGE="set yrange ${YRANGE:-[95:170]}"
fi

cat > "boxes-$COL-$PREC.tex" <<EOF
\\begin{tabular}{r|ccccc}
  \\multicolumn{6}{c}{{\\bf ${T[$COL]}${T[$PREC]}}} \\\\
  & Min & 1st q. & Median & 3rd q. & Max \\\\ \\hline\\hline
  `sed -re 's/,/ \& /g;2,$s/^/\\\\\\\\ /;s/"//g' < "$XX"`
\\end{tabular}
EOF


gnuplot /dev/stdin <<EOF
  set terminal pdf enhanced background rgb 'white';
  set datafile separator ',';
  set output 'boxes-$COL-$PREC.pdf';

  set style fill solid 0.25 border -1
  set style boxplot fraction 1
  set style data boxplot
  set boxwidth 7 absolute

  set xlabel "Processed terminals (in %)"
  set ylabel "${YLABEL:-Relative weight of solution}"

  set title '${T[$COL]}${T[$PREC]}' font 'Arial,14'
  set xtics 0,10,100 scale 0
  set xrange [-10:110]
  $YRANGE
  $OPTS

  plot '$XX' using 1:3:2:6:5 with candlesticks lt 3 notitle whiskerbars 0.5, \
       ''    using 1:4:4:4:4 with candlesticks lt -1 notitle
EOF

