package require tkpath 0.3.0
 
set t .c_imageanchor
toplevel $t
set w $t.c
pack [tkp::canvas $w -width 960 -height 960 -bg "#c6ceef" -highlightthickness 0]


namespace eval ::imageanchor {
    variable w $::w

    set dir [file dirname [info script]]
    set imageFile [file join $dir trees.gif]
    set name [image create photo -file $imageFile]
    set x0 230
    set y0 230
    set dx 250
    set dy 250

    set anchors [list nw n ne w c e sw s se]
    for {set i 0} {$i < [llength $anchors] } {incr i} {
        set a [lindex $anchors $i]
        set x [expr $x0 + $i%3 * $dx]
        set y [expr $x0 + $i/3 * $dy]
        set t $a
        $w create pimage $x $y -image $name -anchor $a -tags $a
        $w create ptext $x $y -text $t -fontfamily "Times" -fontsize 23 -fill black -stroke white -strokewidth 3 -filloverstroke 1 -textanchor middle
        $w create path "M $x $y m -5 0 h 10 m -5 -5 v 10" -stroke red
        $w bind $a <Enter> "puts enter-$a"
        $w bind $a <Leave> "puts leave-$a"
    }

    proc ticker {deg step tim} {
        variable w
        variable anchors
        variable x0
        variable y0
        variable dx
        variable dy
        if {[winfo exists $w]} {
            after $tim [list imageanchor::ticker [expr [incr deg $step] % 360] $step $tim]
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

    ticker 0 1 50

}