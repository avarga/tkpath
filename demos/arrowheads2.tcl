package require tkpath 0.3.3

set t .c_arrowheads2
toplevel $t
set w $t.c

set width 1120
pack [tkp::canvas $w -width $width -height 800 -bg white]

set yoffset 20
set xoffset 20
set lwid 1
set fillcol 0
set xlen 70
set xdelta 100
set arrowwidth 9

foreach startarrow {on} {
    foreach endarrow {on} {
        foreach arrowlength {12.0 0.0 -12.0} {
            foreach arrowfill {0.0 0.5 1.0 1.5} {
                foreach cap {butt projecting round} {
                    foreach join {bevel miter round} {
                        foreach smoo {no yes} {
                            set colname [lindex {blue green red brown black magenta cyan} $fillcol]

                            set x1 $xoffset
                            set y1 [expr $yoffset + 0]
                            set x2 [expr $xoffset + $xlen]
                            set y2 [expr $yoffset + 30]
                            $w create pline $x1 $y1 $x2 $y2 \
                                    -stroke $colname \
                                    -strokelinecap $cap \
                                    -strokelinejoin $join\
                                    -startarrow $startarrow \
                                    -startarrowfill $arrowfill \
                                    -startarrowlength $arrowlength \
                                    -startarrowwidth $arrowwidth \
                                    -endarrow $endarrow \
                                    -endarrowfill $arrowfill \
                                    -endarrowlength $arrowlength \
                                    -endarrowwidth $arrowwidth \
                                    -strokewidth $lwid \
                                    -strokeopacity 0.5 \

                            $w create pline $x1 $y1 $x2 $y2 \

                            set lwid [expr ($lwid%11)+1]
                            set fillcol [expr ($fillcol+1)%7]
                            incr xoffset $xdelta
                            if {$xoffset >= ($width - $xlen)} {
                                set xoffset 20
                                incr yoffset 30
                            }
                        }
                    }
                }
            }
        }
    }
}
