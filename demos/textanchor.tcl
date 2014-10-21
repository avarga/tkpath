package require tkpath 0.3.3

set t .c_textanchor
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 800 -height 800 -bg "#c6ceef" -highlightthickness 0]


namespace eval ::textanchor {
    variable w $::w
    variable x0
    variable y0
    variable dx
    variable dy

    set x0 150
    set y0 150
    set dx 250
    set dy 100

    set anchors [list nw n ne w c e sw s se start middle end]

    set fs 30
    set q "XyX"
    set s "-"

    for {set i 0} {$i < [llength $anchors] } {incr i} {
        set a [lindex $anchors $i]
        set x [expr $x0 + $i%3 * $dx]
        set y [expr $x0 + $i/3 * $dy]
        set t "$q$s$a$s$q"
        $w create ptext $x $y -text $t -fontfamily Times -fontsize $fs -fill white -textanchor $a -tags $a
        $w create path "M $x $y m -5 0 h 10 m -5 -5 v 10" -stroke red
        $w bind $a <Enter> "puts enter-$a"
        $w bind $a <Leave> "puts leave-$a"
    }
    set phi [expr 15.0/180 * 3.14]

    proc ticker {deg step tim} {
        variable w
        variable anchors
        variable x0
        variable y0
        variable dx
        variable dy
        if {[winfo exists $w]} {
            after $tim [list textanchor::ticker [expr [incr deg $step] % 360] $step $tim]
            set phi [expr 2*$deg*3.14159/360.0]
            for {set i 0} {$i < [llength $anchors] } {incr i} {
                set a [lindex $anchors $i]
                set x [expr $x0 + $i%3 * $dx]
                set y [expr $y0 + $i/3 * $dy]
                set m [::tkp::transform rotate $phi $x $y]
                $w itemconfig $a -m $m
            }
        }
    }

    ticker 0 2 40

}