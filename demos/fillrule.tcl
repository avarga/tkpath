# fillrule.tcl

toplevel ._fillrule
set w ._fillrule.c
pack [canvas $w]
$w create path "M 10 10 h 80 v 80 h -80 z m 20 20 h 40 v 40 h -40 z" \
  -fill green -fillrule nonzero

set id [$w create path "M 10 10 h 80 v 80 h -80 z m 20 20 h 40 v 40 h -40 z" \
  -fill blue -fillrule evenodd]
$w move $id 100 0

