package require tkpath 0.3.0

set t .c_imagealpha
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 600 -height 700 -bg white]

set dir [file dirname [info script]]
set imageFile [file join $dir trees.gif]
set name [image create photo -file $imageFile]
set imageFile2 [file join $dir zoom_sh.png]
set img2 [image create photo -file $imageFile2]

$w create ptext 10 50 -text "image"
$w create image 250 150 -image $name -anchor c
$w create image 450 150 -image $name -anchor c
$w create prect 150 110 550 190 -fill purple -fillopacity 0.5 -stroke ""
$w create image 250 180 -image $img2 -anchor c

$w create ptext 10 400 -text "pimage"
$w create pimage 250 500 -image $name -anchor c
$w create pimage 450 500 -image $name -anchor c
$w create prect 150 460 550 540 -fill purple -fillopacity 0.5 -stroke ""
$w create pimage 250 530 -image $img2 -anchor c
