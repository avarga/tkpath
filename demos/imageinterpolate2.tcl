package require tkpath 0.3.2

set t .c_imageinterpolate2
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 800 -height 800 -bg white]

set dir [file dirname [info script]]
set imageFile [file join $dir trees.gif]
set name [image create photo -file $imageFile]
set x 10
set y 10
set rx1 0
set rx2 50
set ry1 340
set ry2 400

$w create pimage $x $y -image $name -srcregion [list $rx1 $ry1 $rx2 $ry2 ]

$w create prect $x $y \
  [expr $x + $rx2 - $rx1] [expr $y + $ry2 -$ry1]

set wi [expr 8 * ($rx2 - $rx1)]
set h  [expr 8 * ($ry2 - $ry1)]

$w create pimage 400 0 -image $name -width $wi -height $h -interpolation none -srcregion [list $rx1 $ry1 $rx2 $ry2 ]

$w create pimage 0 400 -image $name -width $wi -height $h -interpolation fast -srcregion [list $rx1 $ry1 $rx2 $ry2 ]

$w create pimage 400 400 -image $name -width $wi -height $h -interpolation best -srcregion [list $rx1 $ry1 $rx2 $ry2 ]
