#!/bin/bash

plot() {
  XX="`tempfile`"

  trap 'rm -f "$XX"' EXIT

  grep ',@@' > "$XX" < "$1"
  fin="`sed -nre 's/## FINAL WEIGHT (.*)/\1/p' < "${1%.zel.log}.nozel.log"`"

  gnuplot -e "
    min(x, y) = (x < y) ? x : y;
    max(x, y) = (x > y) ? x : y;

    set terminal svg enhanced background rgb 'white';
    set datafile separator ',';
    set output '$2';
    set xrange [-5:105];

    stats '$XX' using 5 name 'term' nooutput;
    T(x) = 100 * (term_max - x) / (term_max - term_min);

    stats '$XX' using 10 name 'C10' nooutput;
    stats '$XX' using 11 name 'C11' nooutput;
    stats '$XX' using 12 name 'C12' nooutput;
    stats '$XX' using 13 name 'C13' nooutput;
    stats '$XX' using 14 name 'C14' nooutput;

    y_min = min(min(C10_min, min(C11_min, min(C12_min, min(C13_min, C14_min)))), $fin);
    y_max = max(C10_max, max(C11_max, max(C12_max, max(C13_max, C14_max))));
    Y(y) = 100 * y / y_min;
    y_r = Y(y_max) - 100;

    set yrange [100 - y_r / 20 : Y(y_max) + y_r / 20 ];

    plot '$XX' using (T(\$5)):(Y(\$10)) with lines title 'MST',
            '' using (T(\$5)):(Y(\$11)) with lines title 'MST+',
            '' using (\$0 == 1 ? T(\$5) : NaN):(Y(\$10)) with points notitle,
            '' using (T(\$5)):(Y($fin)) with lines title 'fin',
            '' using (T(\$5)):(Y(\$12)) with lines title 'Zelikovsky',
            '' using (T(\$5)):(Y(\$13)) with lines title 'Zelikovsky-',
            '' using (T(\$5)):(Y(\$14)) with lines title 'Zelikovsky+'
  "
}

skipped=0
skipped_files=()

mkdir -p plots-zel
cd logs
for l in *.noprec.zel.log; do
  if grep -q "FINAL WEIGHT" < $l; then
    plot $l ../plots-zel/${l%log}svg
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

