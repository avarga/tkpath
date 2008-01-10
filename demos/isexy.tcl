package require tkpath 0.2.8
if {[tk windowingsystem] ne "aqua"} {
    return
}

set t .c_isexy1
destroy $t
toplevel $t
wm attributes $t -transparent 1
set w $t.c
pack [canvas $w -width 400 -height 400 -bg systemTransparent -highlightthickness 0] \
  -fill both -expand 1

set ::tkpath::antialias 1

set height 24
set width 120
set radius 12

array set light {
    gray  "#e6e6e6"
    blue  "#d6d6ff"
    green "#cae2c5"
    red   "#ffd6d6"
}

proc drawcolumn {w tag {op 1.0}} {
    global  height width radius light
    
    array set font [font actual systemSystemFont]
    set family $font(-family)
    set fsize $font(-size)

    set g1 [tkpath::gradient create linear \
      -stops [list [list 0.0 gray90 $op] [list 1.0 gray60 $op]] \
      -lineartransition {0 0 0 1}]

    set ybase [expr {$height - ($height - $fsize)/2 - 1}]

    set id1 [$w create prect 0 0 $width $height -rx $radius \
      -fill $g1 -stroke gray50 -tags $tag -strokeopacity $op]
    set id2 [$w create ptext 20 $ybase -fontfamily $family -fontsize $fsize \
      -text "Vikings" -tags $tag -fill white -fillopacity $op]
    set id3 [$w create ptext 20 $ybase -fontfamily $family -fontsize $fsize \
      -text "Vikings" -tags $tag -fillopacity $op]

    $w move $id1 0.5 0.5
    $w move $id2 0 1
    
    set y 0
    foreach col {gray blue green red} text {Tor Freja Fro Oden} {
	incr y [expr {$height + 8}]
	set id1 [$w create prect 0 0 $width $height -rx $radius \
	  -fill $light($col) -stroke "" -tags $tag -fillopacity $op]
	set id2 [$w create ptext 20 $ybase -fill gray30 -text $text -tags $tag \
	  -fillopacity $op -fontfamily $family -fontsize $fsize]
	$w move $id1 0 $y
	$w move $id2 0 $y
    }
}
    
proc drawbar {w tag} {
    
    set g1 [tkpath::gradient create linear \
      -stops {{0.0 "#c3c3c3"} {1.0 "#969696"}} \
      -lineartransition {0 0 0 1}]
    $w create prect 0 0 2000 40 -fill $g1 -stroke "" -tags $tag
    $w create pline 0 40.5 2000 40.5 -stroke "#404040"    
}

proc drawbutton {win grad tag {type plain}} {
    
    set w 26
    set h 21
    set r 4
    set a [expr {$w-2*$r}]
    set b [expr {$h-2*$r}]
    set c [expr {$w-$r}]
    
    switch -- $type {
	plain {
	    set p "M $r 0 h $a q $r 0 $r $r v $b q 0 $r -$r $r h -$a q -$r 0 -$r -$r v -$b q 0 -$r $r -$r Z"
	}
	left {
	    set p "M $r 0 h $c v $h h -$c q -$r 0 -$r -$r v -$b q 0 -$r $r -$r Z"
	}
	center {
	    set p "M 0 0 h $w v $h h -$w z"
	}
	right {
	    set p "M 0 0 h $c q $r 0 $r $r v $b q 0 $r -$r $r h -$c z"
	}
    }
    set id1 [$win create path $p -stroke "#c2c2c2" -tags $tag -fill ""]
    set id2 [$win create path $p -stroke "#454545" -tags $tag -fill $grad]
    $win move $id1 0 1
    $win move $id1 0.5 0.5
    $win move $id2 0.5 0.5
}

drawcolumn $w c1
$w move c1 10 60
drawcolumn $w c2 0.6
$w move c2 [expr {$width + 2*10}] 60

drawbar $w bar

set g [tkpath::gradient create linear \
  -stops {{0.0 "#ffffff"} {0.5 "#d1d1d1"} {1.0 "#a9a9a9"}} \
  -lineartransition {0 0 0 1}]

drawbutton $w $g b1
set l 12
set path "M 0 0 h $l M 0 3 h $l M 0 6 h $l M 0 9 h $l"
set id [$w create path $path -stroke "#0f0f0f" -tags b1]
$w move $id 8 6
$w move $id 0 0.5
set id [$w create path $path -stroke white -strokeopacity 0.5 -tags b1]
$w move $id 8 7
$w move $id 0 0.5
$w move b1 20 10

drawbutton $w $g b2 left
set path "M 0 0 l 9 4.5 v -9 z"
set id [$w create path $path -fill white -stroke "" -tags {b2 b2-a}]
$w move $id 0 0.5
set id [$w create path $path -fill black -stroke "" -tags {b2 b2-a} -fillopacity 0.7]
$w move b2-a 8 10.5
$w move b2 60 10

drawbutton $w $g b3 right
set path "M 0 0 l -9 4.5 v -9 z"
set id [$w create path $path -fill white -stroke "" -tags {b3 b3-a}]
$w move $id 0 0.5
set id [$w create path $path -fill black -stroke "" -tags {b3 b3-a} -fillopacity 0.78]
$w move b3-a 17 10.5
$w move b3 [expr {60+26}] 10

proc drawletter {w c tag} {
    array set font [font actual systemSystemFont]
    set family $font(-family)
    set fsize $font(-size)

    $w create ptext 10 17 -fontfamily $family -fontsize $fsize -tags $tag \
      -text $c -fill white
    $w create ptext 10 16 -fontfamily $family -fontsize $fsize -tags $tag \
      -text $c -fillopacity 0.8
}

drawbutton $w $g b4 left
drawbutton $w $g b5 center
drawbutton $w $g b6 center
drawbutton $w $g b7 right
drawletter $w M b4c
drawletter $w A b5c
drawletter $w T b6c
drawletter $w S b7c
$w move b4 [expr {130+0*26}] 10
$w move b5 [expr {130+1*26}] 10
$w move b6 [expr {130+2*26}] 10
$w move b7 [expr {130+3*26}] 10
$w move b4c [expr {130+0*26}] 10
$w move b5c [expr {130+1*26}] 10
$w move b6c [expr {130+2*26}] 10
$w move b7c [expr {130+3*26}] 10

lassign [$w bbox all] x y wb hb





