package require tkpath 0.3.3

set t .c_imageregion
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 850 -height 800 -bg white]

set dir [file dirname [info script]]
set imageFile [file join $dir trees.gif]
set name [image create photo -file $imageFile]

set iwidth  [image width $name]
set iheight [image height $name]
set halfiwidth  [expr $iwidth / 2]
set halfiheight [expr $iheight / 2]


# Original image
set x 20
set y 20

$w create ptext $x [expr $y - 5] -text "Original image"
$w create pimage [expr $x + 1] [expr $y + 1] -image $name -width $iwidth -height $iheight
$w create prect $x $y [expr $x + $iwidth] [expr $y + $iheight] -stroke green


# Cropped region
set x 200
set y 20
set rx1 20
set rx2 70
set ry1 20
set ry2 70
$w create ptext $x [expr $y - 5] -text "pimage -srcregion {$rx1 $ry1 $rx2 $ry2}"
$w create pimage [expr $x + 1] [expr $y + 1] -image $name -srcregion [list $rx1 $ry1 $rx2 $ry2]
$w create prect $x $y [expr $x + $rx2 - $rx1] [expr $y + $ry2 - $ry1] -stroke blue
incr y $ry2
$w create pimage [expr $x + 1] [expr $y + 1] -image $name -width $iwidth -height $iheight
$w create prect $x $y [expr $x + $iwidth] [expr $y + $iheight] -stroke green
$w create prect [expr $x + $rx1] [expr $y + $ry1] [expr $x + $rx2] [expr $y + $ry2] -stroke blue


# Scaled to 1.5, topleft and rightbottom drawn with -srcregion
set x 400
set y  50

set width [expr 1.5 * $iwidth]
set height [expr 1.5 * $iheight]
set halfwidth  [expr $width / 2]
set halfheight [expr $height / 2]

$w create ptext $x [expr $y - 5] -text "Scaled to 1.5, topleft and rightbottom drawn with -srcregion"
$w create pimage $x $y -image $name -width $width -height $height -tintcolor yellow -tintamount 0.5
$w create pimage $x $y -image $name \
        -width $halfwidth -height $halfheight \
        -srcregion [list  0  0  $halfiwidth $halfiheight ]
$w create pimage [expr $x + $halfwidth] [expr $y + $halfheight] -image $name \
        -width $halfwidth -height $halfheight \
        -srcregion [list $halfiwidth  $halfiheight  $iwidth $iheight ]
$w create prect [expr $x -1] [expr $y - 1] [expr $x + $width] [expr $y + $height] -stroke red


# Scaled to 1.5, -srcregion <full image>
set x 600
set y 75

$w create ptext $x [expr $y - 5] -text "Scaled to 1.5, -srcregion <full image>"
$w create pimage $x $y -image $name -width $width -height $height -srcregion [list 0  0  $iwidth $iheight ]
$w create prect [expr $x -1] [expr $y - 1] [expr $x + $width] [expr $y + $height] -stroke blue


# Tiled with 5x5 images, topleft corner is the center of a tile image
set x 70
set y 350
set width [expr 2 * $iwidth]
set height [expr 2 * $iheight]
$w create ptext $x [expr $y - 5] -text "Tiled with 5x5 images, topleft corner is the center of a tile image"
$w create pimage $x $y -image $name -width $width -height $height -srcregion [list $halfiwidth $halfiheight  [expr $halfiwidth + 5 * $iwidth]  [expr $halfiheight + 5 * $iheight] ]
$w create prect [expr $x -1] [expr $y - 1] [expr $x + $width] [expr $y + $height] -stroke purple
