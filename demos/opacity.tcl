#load /Users/matben/C/cvs/tkpath/macosx/build/tkpath0.1.dylib
package require tkpath

set t .c_opacity
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1

set r 40
set d [expr 2*$r]
set opacity 0.50
set rc 100

foreach col {red green blue} {
    $w create path "M 0 0 A $r $r 0 1 0 0 $d A $r $r 0 1 0 0 0 z" \
      -stroke "" -fill $col -fillopacity $opacity -tags $col
}

$w move all 200 [expr 200-$r]

set time 0
set speed 0.1

proc move {} {
    global w rc time speed
    
    if {![winfo exists $w]} {
	return
    }
    set phi [expr $time*$speed]
    array set frac [list red 3./7 green 11./17. blue 23./29.]
    foreach col {red green blue} {
	set tx [expr $rc*cos([expr $phi*$frac($col)])]
	set ty [expr $rc*sin([expr $phi/$frac($col)])]
	set m [list {1 0} {0 1} [list $tx $ty]]
	$w itemconfig $col -matrix $m
    }
    incr time
    after 200 move
}




