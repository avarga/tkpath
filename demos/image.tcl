package require tkpath

set t .c_image
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1

set dir [file dirname [info script]]
set f [file join $dir trees.gif]
set name [image create photo -file $f]
set x 20
set y 20
$w create pimage $x $y -image $name

set id [$w create prect $x $y  \
  [expr $x+[image width $name]] [expr $y+[image height $name]]]
$w move $id 0.5 0.5

set m [::tkpath::transform rotate 0.5]
lset m {2 0} 220
lset m {2 1} -120
$w create pimage 100 100 -image $name -matrix $m

set m [::tkpath::transform scale 2 0.8]
$w create pimage 10 300 -image $name -matrix $m




