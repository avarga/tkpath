package require tkpath

set t .c_prect
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1


$w create prect 20.5  20.5 180.5 80.5 -rx 6 -stroke "#c8c8c8" -fill "#e6e6e6"
$w create prect 200.5 20.5 260.5 80.5 -rx 6 -stroke "#a19de2" -fill "#d6d6ff"

$w create prect 20.5  100.5 180.5 180.5 -rx 6 -stroke "#9ac790" -fill "#cae2c5"
$w create prect 200.5 100.5 260.5 180.5 -rx 6 -stroke "#e2a19d" -fill "#ffd6d6"

$w create prect 20.5 200.5 260.5 380.5 -stroke "#999999"
$w create prect 40.5 220.5 100.5 360.5 -rx 16 -stroke "#666666" -strokewidth 3 -fill "#bdbdbd"

$w create prect 150 240 170 260 -stroke "" -fill red
$w create prect 150 270 170 290 -stroke "" -fill green
$w create prect 150 300 170 320 -stroke "" -fill blue

