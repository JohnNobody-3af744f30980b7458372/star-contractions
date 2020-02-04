#!/bin/bash

check_file() {
  if [[ -e "$1" ]] && grep -q FINAL\ WEIGHT < "$1" &&
      ! grep -q 'contract_till_the_bitter_end interrupted' < "$1"; then
    return 0
  fi

  return 1
}

mstp_loops() {
  cat "$@" |
  sed -nre 's/^  refine_solution: new weight (.*) \(was (.*)\) in .*/\1 \2/p' |
  python -c 'if True:
    import sys
    calls = 0
    loops = 0
    for l in sys.stdin:
      b, e = l.split()
      loops += 1
      calls += (b == e)
    print(loops / calls)
  '
}

improved_stars_speed() {
  for f in "$@"; do
    python -c "print(`sed -nre '/contract_till_the_bitter_end:/s/.* (.*) s/\1/p' < "$f.prec.nozel.log"` / `sed -nre '/contract_till_the_bitter_end:/s/.* (.*) s/\1/p' < "$f.noprec.nozel.log"`)";
  done | python -c 'if True:
    import sys

    s = 0
    c = 0

    for l in sys.stdin:
      s += float(l)
      c += 1

    print(s/c)
  '
}

get-insts() {
  echo cd "$1"
  cd "$1"

  insts=()
  readarray -t insts <<<"$(
    ls logs/*.log |
    sed -re 's/\.(no)?prec\.(no)?zel\.log$//' |
    while read f; do
      if check_file "$f".noprec.nozel.log &&
         check_file "$f".prec.nozel.log; then
         echo "$f"
      fi
    done | sort -u
  )"

  cd -
}

plot-all() {(
  echo cd "$1"
  cd "$1"

  echo "MST+ avg. loops $(mstp_loops "${insts[@]/%/.noprec.nozel.log}")"
  echo "Improved stars speed $(improved_stars_speed "${insts[@]}")"

  for col in mst mstp; do
    if grep -q "zel" <<<"$col"; then
      ZEL=zel
    else
      ZEL=nozel
    fi

    for prec in prec noprec; do
      if [[ prec == $prec && $col == zel* ]]; then
        continue
      fi
      echo YRANGE="$2" COL=$col PREC=$prec ./plot-boxes.sh
      YRANGE="$2" COL=$col PREC=$prec ./plot-boxes.sh "${insts[@]/%/.$prec.$ZEL.log}"

      Rscript --vanilla ../process.R "$col" avg2 "${insts[@]/%/.$prec.$ZEL.log}" > avg-$prec-$col.txt
    done
  done

  gnuplot /dev/stdin <<EOF
    set terminal pdf enhanced background rgb 'white';
    set datafile separator ',';
    set output 'avg.pdf';
 
    set xlabel "Processed terminals (in %)"
    set ylabel "${YLABEL:-Relative weight of solution}"
 
    set title 'Average solution quality' font 'Arial,14'
    set xtics 0,10,100 scale 0
    set xrange [-10:110]
    $OPTS
 
    plot 'avg-noprec-mst.txt' using 1:2 with lines title 'MST', \
         'avg-noprec-mstp.txt' using 1:2 with lines title 'MST+', \
         'avg-prec-mst.txt' using 1:2 with lines title 'MST (Improved stars)', \
         'avg-prec-mstp.txt' using 1:2 with lines title 'MST+ (Improved stars)'

EOF

  (
    echo "\\begin{tabular}{r|cccc}
      \\multicolumn{8}{c}{{\\bf Averages }} \\\\
      & MST & MST+ & MST (I) & MST+ (I) \\\\ \\hline\\hline"

    for i in {0..10}; do
      echo "$((i*10))"
      for f in avg-noprec-mst{,p}.txt avg-prec-mst{,p}.txt; do
        echo -n '& '
        sed -n '1~10p' < "$f" | sed -nre "s/\"//g;$((i+1))s/.*,//p"
      done
      echo '\\'
    done

    echo "\\end{tabular}"
  ) > avg.tex
)}

get-insts 010-nozel
plot-all 010-nozel

cd 011-nozel-work
for col in W1 W2 W1c W2c; do
  for prec in prec noprec; do
    if [[ $col == W1 ]]; then
      export YRANGE=[-0.1:1.1] \
             YLABEL="Recalculated stars (fraction)"
    else
      export YRANGE=[-0.1:5.1] \
             YLABEL="Visited vertices (fraction of N^2)"
    fi
    OPTS="" COL=$col PREC=$prec ./plot-boxes.sh "${insts[@]/%/.$prec.nozel.log}"
    Rscript --vanilla ../process.R "$col" avg2 "${insts[@]/%/.$prec.nozel.log}" > avg-$prec-$col.txt
  done
done

for W in W1 W2 W1c W2c; do
  case $W in
    W1)
      title="recalculated ratios"
      ylabel="Recalculated stars (fraction of N)";;
    W2)
      title="visited vertices"
      ylabel="Visited vertices (fraction of N^2)";;
  esac

  case $W in
    *c) yrange='[-0.05:0.45]';;
    *)  yrange='[-0.05:1.05]';;
  esac

  gnuplot /dev/stdin <<EOF
    set terminal pdf enhanced background rgb 'white';
    set datafile separator ',';
    set output 'avg-$W.pdf';
    
    set xlabel "Processed terminals (in %)"
    set ylabel "$ylabel"
    
    set title 'Average work: $title' font 'Arial,14'
    set xtics 0,10,100
    set xrange [-10:110]
    set yrange $yrange
    $OPTS
    
    plot 'avg-noprec-$W.txt' using 1:2 with lines title 'Normal stars', \
         'avg-prec-$W.txt' using 1:2 with lines title 'Improved stars'
EOF

  (
    echo "\\begin{tabular}{r|cc}
      \\multicolumn{3}{c}{{\\bf Averages: $title }} \\\\
      & Normal & Improved \\\\ \\hline\\hline"

    for i in {0..10}; do
      echo "$((i*10))"
      for f in avg-{,no}prec-$W.txt; do
        echo -n '& '
        sed -n '1~10p' < "$f" | sed -nre "s/\"//g;$((i+1))s/.*,//p"
      done
      echo '\\'
    done

    echo "\\end{tabular}"
  ) > avg-$W.tex
done

paste -d, ../010-nozel/avg-noprec-mstp.txt avg-noprec-W2c.txt > iw-noprec.txt
paste -d, ../010-nozel/avg-prec-mstp.txt avg-prec-W2c.txt > iw-prec.txt

gnuplot /dev/stdin <<EOF
  set terminal pdf enhanced background rgb 'white';
  set datafile separator ',';
  set output 'quality-work.pdf';
  
  set xlabel "Relative quality of solution"
  set ylabel "Total work done"
  
  set title 'Quality-Work comparison' font 'Arial,14'
  set xrange [] reverse
  set yrange [-0.02:0.27]
  $OPTS
  
  plot 'iw-noprec.txt' using 2:4 with lines title 'Normal stars', \
       'iw-prec.txt' using 2:4 with lines title 'Improved stars'
EOF

