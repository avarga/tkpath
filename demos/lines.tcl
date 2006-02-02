package require tkpath

set t .c_lines
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1


$w create pline 20.5  20.5 180.5 20.5 -stroke "#c8c8c8"
$w create pline 200.5 20.5 260.5 20.5 -stroke "#a19de2"

$w create pline 20.5  25.5 180.5 25.5 -stroke "#9ac790"
$w create pline 200.5 25.5 260.5 25.5 -stroke "#e2a19d"

$w create pline 20.5 30.5 260.5 30.5 -stroke "#999999"
$w create pline 40.5 40.5 100.5 40.5 -stroke "#666666" -strokewidth 3

$w create pline 150.5 50.5 170.5 50.5 -stroke red -strokewidth 4
$w create pline 150.5 60.5 170.5 60.5 -stroke green -strokewidth 4
$w create pline 150.5 70.5 170.5 70.5 -stroke blue -strokewidth 4

$w create polyline 20.5 200.5 30.5 200.5 30.5 180.5 50.5 180.5 50.5 200.5  \
  70.5 200.5 70.5 160.5 90.5 160.5 90.5 200.5 110.5 200.5 110.5 120.5 130.5 120.5  \
  130.5 200.5

$w create polyline 150 200 200 120 150 120 200 200  -stroke gray50 -strokewidth 4


