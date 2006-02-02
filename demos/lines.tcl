package require tkpath

set t .c_lines
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1


$w create pline 20.5  20.5 180.5 20.5
$w create pline 200.5 20.5 260.5 20.5 -stroke blue

$w create pline 20.5  30.5 180.5 30.5 -stroke green
$w create pline 200.5 30.5 260.5 30.5 -stroke red

$w create pline 20.5 40.5 260.5 40.5 -stroke "#999999"
$w create pline 40 50 120 80 -stroke "#666666" -strokewidth 3

$w create pline 150.5 60.5 170.5 60.5 -stroke red -strokewidth 4
$w create pline 150.5 70.5 170.5 70.5 -stroke green -strokewidth 4
$w create pline 150.5 80.5 170.5 80.5 -stroke blue -strokewidth 4

$w create polyline 20.5 200.5 30.5 200.5 30.5 180.5 50.5 180.5 50.5 200.5  \
  70.5 200.5 70.5 160.5 90.5 160.5 90.5 200.5 110.5 200.5 110.5 120.5 130.5 120.5  \
  130.5 200.5

$w create polyline 150 200  200 120  150 120  200 200  -stroke gray50 -strokewidth 4
$w create polyline 220 200  270 120  220 120  270 200  -stroke gray50 -strokewidth 4 \
  -fill gray80

$w create ppolygon 75 237  89 280  134 280  98 307  111 350  75 325  38 350  \
  51 307  15 280  60 280 -stroke "#9ac790" -strokewidth 4 -fill "#cae2c5"

$w create ppolygon 240 250  283 275  283 325  240 350  196 325  196 275 \
  -stroke "#a19de2" -strokewidth 6 -fill "#d6d6ff"

$w create text 300  20  -anchor w -text "pline"
$w create text 300 150  -anchor w -text "polyline"
$w create text 300 300 -anchor w -text "ppolygon"

