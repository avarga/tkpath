package require tkpath

set f /Users/matben/Tcl/cvs/coccinella/images/coccinella32.gif
if {![file exists $f]} {
    return
}
set t .c_image
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1

set name [image create photo -file $f]
$w create pimage 100 100 -image $name

set id [$w create prect 100 100  \
  [expr 100+[image width $name]] [expr 100+[image height $name]]]
$w move $id 0.5 0.5

