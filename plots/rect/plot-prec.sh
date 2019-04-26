#!/bin/bash

XX="`tempfile`"
YY="`tempfile`"
trap 'rm -f "$XX" "$YY"' EXIT

plot() {
  grep ',@@' > "$XX" < "$1"
  grep ',@@' > "$YY" < "${1%.prec.nozel.log}.noprec.nozel.log"
  fin="`sed -nre 's/## FINAL WEIGHT (.*)/\1/p' < "$1"`"
  fin2="`sed -nre 's/## FINAL WEIGHT (.*)/\1/p' < "${1%.prec.nozel.log}.noprec.nozel.log"`"

  gnuplot /dev/stdin <<EOF
    min(x, y) = (x < y) ? x : y;
    max(x, y) = (x > y) ? x : y;

    set terminal svg enhanced background rgb 'white';
    set datafile separator ',';
    set output '$2';
    set xrange [-5:105];

    stats '$XX' using 5 name 'term' nooutput;
    T(x) = 100 * (term_max - x) / (term_max - term_min);

    stats '$XX' using 10 name 'CX10' nooutput;
    stats '$XX' using 11 name 'CX11' nooutput;
    stats '$YY' using 10 name 'CY10' nooutput;
    stats '$YY' using 11 name 'CY11' nooutput;

    y_min = min(min(min(min(min(CX10_min, CX11_min), $fin), $fin2), CY10_min), CY11_min);
    y_max = max(max(CX10_max, CX11_max), max(CY10_max, CY11_max));
    Y(y) = 100 * y / y_min;
    y_r = Y(y_max) - 100;

    set yrange [100 - y_r / 20 : Y(y_max) + y_r / 20 ];

    plot '$XX' using (T(\$5)):(Y(\$10)) with lines title 'prec MST', \
            '' using (T(\$5)):(Y(\$11)) with lines title 'prec MST+', \
            '' using (\$0 == 1 ? T(\$5) : NaN):(Y(\$10)) with points notitle, \
            '' using (T(\$5)):(Y($fin)) with lines title 'fin', \
         '$YY' using (T(\$5)):(Y(\$10)) with lines title 'MST', \
            '' using (T(\$5)):(Y(\$11)) with lines title 'MST+', \
            '' using (\$0 == 1 ? T(\$5) : NaN):(Y(\$10)) with points notitle
EOF
}

skipped=0
skipped_files=()

mkdir -p plots-prec
cd logs
for l in *.prec.nozel.log; do
  if grep -q "FINAL WEIGHT" < $l; then
    plot $l ../plots-prec/${l%log}svg
  else
    : $(( skipped++ ))
    skipped_files+=( "$l" )
  fi
done

if (( skipped > 0 )); then
  echo "$skipped files were incomplete and skipped!"
  echo "${skipped_files[*]}"
else
  echo "Done"
fi

