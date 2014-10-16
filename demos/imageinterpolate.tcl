package require tkpath 0.3.0

set t .c_imageinterpolate
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 800 -height 800 -bg white]

set dir [file dirname [info script]]
set imageFile [file join $dir trees.gif]
set name [image create photo -file $imageFile]
set x 0
set y 0
$w create pimage $x $y -image $name

$w create prect $x $y \
  [expr $x+[image width $name]] [expr $y+[image height $name]]

set m [::tkp::transform scale 4 4]
$w create pimage 100 0 -image $name -matrix $m -interpolation none

set m [::tkp::transform scale 4 4]
$w create pimage 0 100 -image $name -matrix $m -interpolation fast

set m [::tkp::transform scale 4 4]
$w create pimage 100 100 -image $name -matrix $m -interpolation best
