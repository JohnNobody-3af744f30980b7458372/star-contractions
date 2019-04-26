#!/bin/bash

# !!!! requrires gnuplot >= 5 !!!!

XX="`tempfile`"
YY="`tempfile`"
trap 'rm -f "$XX" "$YY"' EXIT

preprocess() {
  out="$1"
  shift

  cat "$@" |
  sed -nre 's/.*Star with ([0-9]+) terminals.*/\1/p' |
  sort -n | uniq -c | sed -re 's/^ *//' > "$out"
}

preprocess "$XX" logs/instance*.noprec.nozel.log
preprocess "$YY" logs/instance*.prec.nozel.log

K=10
SEQ=`seq 2 $K`

larger_b=0
while read count size; do
  if (( size <= K )); then
    s_b[$size]=$count
  else
    : $(( larger_b += count ))
  fi
done <"$XX"

larger_i=0
while read count size; do
  if (( size <= K )); then
    s_i[$size]=$count
  else
    : $(( larger_i += count ))
  fi
done <"$YY"


cat > star-sizes.tex <<EOF
\\begin{tabular}{l|`for _ in $SEQ; do echo -n "c"; done`c}
  \\# of terminals `for i in $SEQ; do echo -n " & $i"; done` & \$>$K\$ \\\\ \\hline
  \\# of basic stars `for i in $SEQ; do echo -n " & ${s_b[$i]}"; done` & $larger_b \\\\
  \\# of improved stars `for i in $SEQ; do echo -n " & ${s_i[$i]}"; done` & $larger_i
\\end{tabular}
EOF

gnuplot /dev/stdin <<EOF
    set terminal pdf enhanced background rgb 'white';
    set datafile separator ' ';
    set output 'star-sizes.pdf';
 
    set boxwidth 0.9
    set style fill solid 0.5
    set logscale y
    set xrange [0:132]
    set yrange [0.5:300*1000]
 
    set title "Star sizes$TITLE"
    set xlabel "Star size (# of terminals)"
    set ylabel "# of occurances (logscale)"

    plot "$XX" using 2:1 with impulses lw 5 lt rgb "#00FF00" title "Stars", \\
         "$YY" using 2:1 with impulses lw 1.5 lt rgb "red" title "Improved Stars"
EOF


