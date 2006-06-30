package require tkpath

set t .c_opacity
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1

namespace eval ::opacity {

    variable w $::w

    set r 60
    set d [expr 2*$r]
    set opacity 0.50
    variable rc 100
    
    foreach col {red green blue} {
	$w create path "M 0 0 A $r $r 0 1 0 0 $d A $r $r 0 1 0 0 0 z" \
	  -stroke "" -fill $col -fillopacity $opacity -tags $col
    }
    
    $w move all 200 [expr 200-$r]
    
    variable time 0
    variable speed 0.1
    
    proc step {} {
	variable w 
	variable rc 
	variable time 
	variable speed
	
	if {![winfo exists $w]} {
	    return
	}
	set phi [expr $time*$speed]
	
	set tx [expr $rc*cos([expr $phi*11./17.])]
	set ty [expr $rc*sin($phi)]
	set m [list {1 0} {0 1} [list $tx $ty]]
	$w itemconfig red -matrix $m 
	
	set tx [expr $rc*cos($phi)]
	set ty [expr $rc*sin([expr $phi*3./7.])]
	set m [list {1 0} {0 1} [list $tx $ty]]
	$w itemconfig green -matrix $m
	
	set tx [expr $rc*cos([expr $phi*23./29. + 1.0])]
	set ty [expr $rc*sin([expr $phi + 1.0])]
	set m [list {1 0} {0 1} [list $tx $ty]]
	$w itemconfig blue -matrix $m
	
	incr time
	after 100 opacity::step	
    }
    
    step
}


