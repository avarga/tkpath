# fillrule.tcl

#load /Users/matben/C/cvs/tkpath/macosx/build/tkpath0.1.dylib
package require tkpath

toplevel ._fillrule
set w ._fillrule.c
pack [canvas $w -bg white]
$w create path "M 10 10 h 80 v 80 h -80 z m 20 20 h 40 v 40 h -40 z" \
  -fill green -fillrule nonzero

set id [$w create path "M 10 10 h 80 v 80 h -80 z m 20 20 h 40 v 40 h -40 z" \
  -fill blue -fillrule evenodd]
$w move $id 100 0

$w create text 50 100 -text "nonzero"
$w create text 150 100 -text "evenodd"

