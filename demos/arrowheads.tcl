package require tkpath 0.3.0

set t .c_arrowheads
destroy $t
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 500 -height 800 -bg white]

$w create path "M 20 350 l 50 -25 \
  a 25 25  -30 0 1 50 -25 l 50 -25 \
  a 25 50  -30 0 1 50 -25 l 50 -25 \
  a 25 75  -30 0 1 50 -25 l 50 -25 \
  a 25 100 -30 0 1 50 -25 l 50 -25" \
                        -stroke yellow -strokewidth 2 \
                        -startarrow on \
                        -endarrow on \

$w create path "M 70 325 \
  a 25 25  -30 0 1 50 -25 l 50 -25 \
  a 25 50  -30 0 1 50 -25 l 50 -25 \
  a 25 75  -30 0 1 50 -25 l 50 -25 \
  a 25 100 -30 0 1 50 -25" \
                        -stroke orange -strokewidth 2 \
                        -startarrow on \
                        -endarrow on \

#                        -startarrowlength 45 \
#                        -startarrowwidth 27 \
#                        -startarrowfill 0.70 \
#                        -endarrowlength 50.0 \
#                        -endarrowwidth 29 \
#                        -endarrowfill 0.70 \


$w create path "M 100 100 a 25 25 -30 0 1 50 -25 z" -fill yellow -strokewidth 2 \
                        -startarrow on \
                        -endarrow on \


$w create path "M 180 100 a 25 25  30 0 1 50  25 z" -fill yellow -strokewidth 2 \
                        -startarrow on \
                        -endarrow on \



set r 40
set a 10
set b 6
set b2 [expr {2*$b}]
set r2 [expr {2*$r}]
set ra [expr {$r+$a}]
set a2 [expr {2*$r+$a}]



$w create path "M 0 -$a A $ra $ra 0 1 1 6 $a2" \
  -stroke lightblue -strokewidth 7 -tag circle1 \

$w create path "M 0 $a2 A $ra $ra 0 1 1 -6 -$a" \
  -stroke lightblue -strokewidth 7 -tag circle1 \

$w create path "M 0 0 A $r $r 0 1 1 0 $r2 A $r $r 0 1 1 0 0 Z" \
  -strokewidth 2 -tag circle1 \
                        -startarrow on \
                        -endarrow on \

$w create path "M 0 -$a A $ra $ra 0 1 1 6 $a2" \
  -stroke red -tag circle1 \
                        -startarrow on \
                        -startarrowlength 0 \
                        -endarrow on \

$w create path "M 0 $a2 A $ra $ra 0 1 1 -6 -$a" \
  -stroke red -tag circle1 \
                        -startarrow on \
                        -endarrow on \

$w move circle1 400 220



$w create path "M -$ra 0 A $ra $ra 0 1 1 0 $ra" \
  -stroke lightgreen -strokewidth 10 -tag circle2 \

$w create path "M -$ra 0 A $ra $ra 0 1 1 0 $ra" \
  -stroke red -strokewidth 1 -tag circle2 \
                        -startarrow on \
                        -endarrow on \
                        -startarrowlength 12 \
                        -startarrowwidth 9 \
                        -endarrowlength 12 \
                        -endarrowwidth 9 \

$w create path "M 0 $ra A $ra $ra 0 0 1 -$ra 0" \
  -stroke yellow -strokewidth 10 -tag circle2 \

$w create path "M 0 $ra A $ra $ra 0 0 1 -$ra 0" \
  -stroke blue -strokewidth 1 -tag circle2 \
                        -startarrow on \
                        -endarrow on \
                        -startarrowlength 12 \
                        -startarrowwidth 9 \
                        -endarrowlength 12 \
                        -endarrowwidth 9 \

$w move circle2 400 420



# Make an ellipse around origo and put it in place using a transaction matrix
namespace import ::tcl::mathop::*
proc ellipsepath {x y rx ry} {
    list \
            M $x [- $y $ry] \
            a $rx $ry 0 1 1 0 [*  2 $ry] \
            a $rx $ry 0 1 1 0 [* -2 $ry] \
            Z
}
set Phi [expr {45 / 180.0 * 3.1415926535}]
set cosPhi [expr {cos($Phi)*4}]
set sinPhi [expr {sin($Phi)*4}]
set msinPhi [- $sinPhi]
set matrix \
        [list [list $cosPhi $msinPhi] [list $sinPhi $cosPhi] \
        [list 200 600]]

