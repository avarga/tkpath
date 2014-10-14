package require tkpath 0.3.0

set t .c_imagetint
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 900 -height 1000 -bg white]

set dir [file dirname [info script]]
set imageFile [file join $dir trees.gif]
set name [image create photo -file $imageFile]
set imageFile2 [file join $dir find.png]
set img2 [image create photo -file $imageFile2]


set y 20
$w create ptext  20 $y -text amount -fontsize 16
$w create ptext 150 $y -text original -textanchor middle -fontsize 16
$w create ptext 300 $y -text red -textanchor middle -fontsize 16
$w create ptext 450 $y -text green -textanchor middle -fontsize 16
$w create ptext 600 $y -text blue -textanchor middle -fontsize 16

for {set i 0} {$i <= 5} {incr i} {
    set y [expr 120 + $i * 160]
    set a [expr 0.2 * $i]
    $w create ptext   20 $y -text $a -fontsize 16
    $w create pimage 150 $y -image $name -anchor c
    $w create pimage 300 $y -image $name -tintcolor red   -tintamount $a -anchor c
    $w create pimage 450 $y -image $name -tintcolor green -tintamount $a -anchor c
    $w create pimage 600 $y -image $name -tintcolor blue  -tintamount $a -anchor c
    $w create pimage 750 $y -image $name -anchor c

    $w create pimage 750 [expr $y - 20] -image $img2 -tintcolor red   -tintamount $a -anchor c
    $w create pimage 770 [expr $y + 40] -image $img2 -tintcolor green -tintamount $a -anchor c
    $w create pimage 790 [expr $y - 20] -image $img2 -tintcolor blue  -tintamount $a -anchor c
}
