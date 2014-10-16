package require tkpath 0.3.2

set t .c_gradientsalpha
toplevel $t
set w $t.c
pack [tkp::canvas $w -bg white -width 640 -height 600]

set rainbow [::tkp::gradientstopsstyle rainbow]
set g1 [$w gradient create linear -stops {{0 lightblue} {1 blue}}]
set g1op [$w gradient create linear -stops {{0 lightblue 0.5} {1 blue}}]
set g2 [$w gradient create linear -stops {{0 "#f60"} {1 "#ff6"}} \
  -lineartransition {50 0 160 0} -units userspace]
set g2r [$w gradient create linear -stops {{0 "#f60"} {1 "#ff6"}} \
  -lineartransition {0.25 0 0.8 0}]
set g3 [$w gradient create linear -stops {{0 "#f60"} {1 "#ff6"}} \
  -lineartransition {0 0 0 1}]
set g4 [$w gradient create linear -stops $rainbow]
set g5 [$w gradient create linear -stops {{0 lightgreen 0.8 } {1 green 0.4}}]

set y 10
$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g1 -fillopacity 0.5
$w create text 220 20 -anchor w -text "-stops {{0 lightblue} {1 blue}} -fillopacity 0.5"

set y 70
$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g1op
$w create text 220 [expr $y + 10] -anchor w -text "-stops {{0 lightblue 0.5} {1 blue}}"

incr y 60
$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g2 -fillopacity 0.8
$w create pline 50 $y 50 [expr $y - 5]
$w create pline 160 $y 160 [expr $y - 5]
$w create text 220 [expr $y + 10] -anchor w -text "-stops {{0 #f60} {1 #ff6}}"
$w create text 220 [expr $y + 30] -anchor w -text "-lineartransition {50 0 160 0} -units userspace"

incr y 60
$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g2r -fillopacity 0.8
$w create text 220 [expr $y + 10] -anchor w -text "-stops {{0 #f60} {1 #ff6}}"
$w create text 220 [expr $y + 30] -anchor w -text "-lineartransition {0.25 0 0.8 0}"

incr y 60
$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g5
$w create text 220 $y -anchor w -text "-stops {{0 lightgreen 0.8} {1 green 0.4}}"

$w create path "M 40 200 q 60 -200 120 0 z" -fill $g3 -fillopacity 0.3

incr y 60

$w create prect  5 [expr $y - 5] 215 [expr $y + 25] -fill yellow
$w create prect 10 $y 210 [expr $y + 50] -fill $g4 -fillopacity 0.5
set mrot    [::tkp::transform rotate [expr 3.1415/4] 410 [expr $y + 25]]
$w create prect 310 $y 510 [expr $y + 50] -fill $g4 -matrix $mrot -fillopacity 0.5
$w create text 220 $y -anchor w -text "rainbow -fillopacity 0.5"
$w create ptext 420 $y -textanchor w -text "rainbow" -fontsize 50 -fontweight bold -fill $g4

incr y 120

set g6 [$w gradient create radial -stops {{0 white} {1 black}}]
set g7 [$w gradient create radial -stops {{0 white} {1 black}}  \
  -radialtransition {0.6 0.4 0.5 0.7 0.3}]
set g8 [$w gradient create radial -stops {{0 white} {1 black}}  \
  -radialtransition {0.6 0.4 0.8 0.7 0.3}]

$w create circle 60 $y -r 50 -fill red -stroke ""
$w create circle 60 $y -r 50 -fill $g6 -fillopacity 0.8

$w create circle 200 $y -r 50 -fill red -stroke ""
$w create circle 200 $y -r 50 -fill $g7 -fillopacity 0.8 -stroke ""

$w create circle 340 $y -r 50 -fill red -stroke ""
$w create circle 340 $y -r 50 -fill $g8 -fillopacity 0.8 -stroke ""


incr y 120

$w create prect  10 [expr $y - 55] 550 [expr $y + 55] -fill lightblue
set transmit [$w gradient create radial -stops {{0 red 0.8} {1 red 0.0}}]
$w create circle 160 $y -r 50 -fill $transmit -stroke ""


proc GradientsOnButton {w} {
    set id [$w find withtag current]
    if {$id ne ""} {
        set type [$w type $id]
        switch -- $type {
            prect - path - circle - ellipse {
                set stroke [$w itemcget $id -stroke]
                set fill [$w itemcget $id -fill]
                puts "Hit a $type with stroke $stroke and fill $fill"
            }
        }
    }
}
$w bind all <Button-1> [list GradientsOnButton $w]


