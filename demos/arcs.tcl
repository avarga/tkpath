package require tkpath

set t .c_arcs
toplevel $t
set w $t.c
pack [canvas $w -width 500 -height 400 -bg white]

set ::tkpath::antialias 1

$w create path "M 20 350 l 50 -25 \
  a 25 25  -30 0 1 50 -25 l 50 -25 \
  a 25 50  -30 0 1 50 -25 l 50 -25 \
  a 25 75  -30 0 1 50 -25 l 50 -25 \
  a 25 100 -30 0 1 50 -25 l 50 -25" -stroke red -strokewidth 2

$w create path "M 30 350 h 100 a 25 200 0 0 1 50 0 h 200" \
  -stroke blue -strokewidth 2

$w create path "M 100 100 a 25 25 -30 0 1 50 -25 z" -fill yellow -strokewidth 2
$w create path "M 180 100 a 25 25  30 0 1 50  25 z" -fill yellow -strokewidth 2


