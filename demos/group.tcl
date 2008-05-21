package require tkpath 0.3.0


set t .c_group
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 400 -height 400 -bg white]

$w create prect 10 10 300 200
$w create prect 15 15 295 195
$w create prect 20 20 290 190
$w create prect 25 25 285 185

$w create group -tags g1
$w create prect 20 20 40 40 -rx 4 -parent g1
$w create prect 50 50 70 70 -rx 4 -parent g1 -stroke red

$w create group -tags g2
$w create prect 100 20 140 140 -rx 10 -parent g2
$w create prect 150 20 170 70 -rx 4 -parent g2 -stroke blue

return

#set g1 [$w create group -tags g1]
set g1 [$w create group]
$w create prect 20 20 40 40 -rx 4 -parent $g1
$w create prect 50 50 70 70 -rx 4 -parent $g1 -stroke red

#set g2 [$w create group -tags g2]
set g2 [$w create group]
$w create prect 100 20 140 140 -rx 10 -parent $g2
$w create prect 150 20 170 70 -rx 4 -parent $g2 -stroke blue




