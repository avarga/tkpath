#package require tkpath
 load /Users/matben/C/cvs/tkpath/macosx/build/tkpath0.1.dylib

set t .c_hittest
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400]

set ::tkpath::antialias 1

set id [$w create path "M 20 20 L 120 20 v 30 h -20 z"]
$w bind $id <Button-1> [list puts "hit $id"]

set id [$w create path "M 10 80 h 100 v 100 z" -fill blue]
$w bind $id <Button-1> [list puts "hit $id (blue triangle)"]

set id [$w create path "M 20 200 Q 50 120 100 200 T 150 200 200 200"]
$w bind $id <Button-1> [list puts "hit $id (quad bezier)"]

if {0} {
$w create path "M 20 40 h 20 v 10 h -20 z" -fill green

$w create path "M 10 10 h 380 v 380 h -380 z" -stroke blue
$w create path "M 80 120 h -40 a 40 40 0 1 0 40 -40 z" \
  -fill red -stroke blue -strokewidth 4
$w create path "M 70 110 v -40 a 40 40 0 0 0 -40 40 z" \
  -fill yellow -stroke blue -strokewidth 4
$w create path "M 10 200 q 40 -80 90 0"
$w create path "M 20 200 Q 50 120 100 200 T 150 200"
$w create path "M 20 200 Q 50 120 100 200 T 150 200 200 200"

$w create path "M 600 350 l 50 -25  \
  a 25 25 -30 0 1 50 -25 l 50 -25  \
  a 25 50 -30 0 1 50 -25 l 50 -25  \
  a 25 75 -30 0 1 50 -25 l 50 -25  \
  a 25 100 -30 0 1 50 -25 l 50 -25" -stroke red -strokewidth 5

}


