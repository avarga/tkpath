package require tkpath 0.3.3

set t .c_textproperties
toplevel $t
set w $t.c
set bg "#c6ceef"
set stroke $bg
pack [tkp::canvas $w -width 800 -height 800 -bg $bg -highlightthickness 0]

set slantlist [list normal italic oblique ]
set weightlist [list normal bold ]

# -fontweight normal|bold -fontslant normal|italic|oblique
$w create ptext 40 40 -text "-fontweight normal|bold -fontslant normal|italic|oblique :"
for {set i 0} {$i < [llength $slantlist] } {incr i} {
    set x [expr 150 + $i * 250]
    set slant [lindex $slantlist $i]
    for {set j 0} {$j < [llength $weightlist] } {incr j} {
        set y [expr 80 + $j * 50]
        set weight [lindex $weightlist $j]
        set t "$slant $weight"
        set c [expr ($i == 1)?"blue":"yellow"]
        $w create ptext $x $y -text $t -fontfamily Times -fontsize 40 -fontweight $weight -fontslant $slant \
          -fill $c -textanchor middle
    }
}

# -fill
set x 20
set y 200
$w create ptext $x $y  -text "Fill" -fontfamily Helvetica -fontsize 40 -fontweight bold \
    -fill blue -fillopacity 1.0 \

# stroke without fill
set x 20
for {set i 1} {$i <= 4} {incr i} {
    set y [expr 240 + ($i-1) * 40]
    set s [expr 2 * $i]
    $w create ptext $x $y -text "Stroke $s" -fontfamily Helvetica -fontsize 40 -fontweight bold \
            -fill "" \
            -stroke green -strokewidth $s
}

# fill + stroke
set x 220
for {set i 1} {$i <= 4} {incr i} {
    set y [expr 240 + ($i-1) * 40]
    set s [expr 2 * $i]
    $w create ptext $x $y -text "Fill+Stroke $s" -fontfamily Helvetica -fontsize 40 -fontweight bold \
            -fill blue -fillopacity 1.0 \
            -stroke green -strokewidth $s
}

# -filloverstroke
set x 490
for {set i 1} {$i <= 4} {incr i} {
    set y [expr 240 + ($i-1) * 40]
    set s [expr 2 * $i]
    $w create ptext $x $y -text "Stroke+Fill $s" -fontfamily Helvetica -fontsize 40 -fontweight bold \
            -fill blue \
            -stroke green -strokewidth $s \
            -filloverstroke 1
}

# -fillopacity
set x 20
set y 430
    $w create pline  [expr $x - 10] $y [expr $x + 280] [expr $y - 20] -strokewidth 5 -stroke yellow
    $w create ptext $x $y -text "Fill Opacity 0.5" -fontfamily Helvetica -fontsize 40 -fontweight bold \
            -fill blue -fillopacity 0.5 \
            -stroke ""

# -strokeopacity
set x 340
    $w create pline  [expr $x - 10] $y [expr $x + 340] [expr $y - 20] -strokewidth 5 -stroke yellow
    $w create ptext $x $y -text "Stroke Opacity 0.5" -fontfamily Helvetica -fontsize 40 -fontweight bold \
            -stroke blue -strokeopacity 0.5 -strokewidth 5 \
            -fill ""

# -filloverstroke
set x 20
set y 470

$w create ptext $x $y -text "text outline with -strokeopacity and -filloverstroke:"
incr y 20

set t1 "Lorem ipsum dolor sit amet, consectetur adipiscing elit"
set t2 "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium"
set sw 5
set fill "blue"
set stroke $bg

proc t {x y str s} {
  global w sw fill stroke
  $w create ptext $x $y  -text $str -fontfamily Helvetica -fontsize $s \
    -fill $fill -fillopacity 1.0
}

proc to {x y str s} {
  global w sw fill stroke
  $w create ptext $x $y  -text $str -fontfamily Helvetica -fontsize $s \
    -fill $fill -fillopacity 1.0 \
    -stroke $stroke -strokewidth $sw -strokeopacity 0.75 \
    -filloverstroke on
}

t  $x $y $t1 12
t  [expr $x + 5] [expr $y + 5] $t2 12

incr y 20
to $x $y $t1 12
to [expr $x + 5] [expr $y + 5] $t2 12

incr y 50
set sw 15
t  $x $y $t1 42
t  [expr $x + 5] [expr $y + 5] $t2 42

incr y 50
to $x $y $t1 42
to [expr $x + 5] [expr $y + 5] $t2 42

