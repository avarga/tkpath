# tkpath.tcl --
#
#       Various support procedures for the tkpath package.
#       
#  Copyright (c) 2005  Mats Bengtsson
#  
# $Id$

namespace eval ::tkpath {}


# ::tkpath::transform --
# 
#       Helper for designing the -matrix option from simpler transformations.
#       
# Arguments:
#       cmd         any of rotate, scale, skewx, skewy, or translate
#       
# Results:
#       a transformation matrix

proc ::tkpath::transform {cmd args} {
    
    set len [llength $args]
    
    switch -- $cmd {
	rotate {
	    set phi [lindex $args 0]
	    set cosPhi [expr cos($phi)]
	    set sinPhi [expr sin($phi)]
	    set msinPhi [expr -1.0*$sinPhi]
	    if {$len == 1} {
		set matrix \
		  [list [list $cosPhi $sinPhi] [list $msinPhi $cosPhi] {0 0}]
	    } elseif {$len == 3} {
		set cx  [lindex $args 1]
		set cy  [lindex $args 2]
		set matrix [list \
		  [list $cosPhi $sinPhi] \
		  [list $msinPhi $cosPhi] \
		  [list [expr -$cx*$cosPhi + $cy*$sinPhi + $cx] \
		  [expr -$cx*$sinPhi - $cy*$cosPhi + $cy]]]
	    } else {
		return -code error "usage: transform rotate angle ?centerX centerY?"
	    }
	}
	scale {
	    if {$len == 1} {
		set sx [lindex $args 0]
		set sy $sx
	    } elseif {$len == 2} {
		set sx [lindex $args 0]
		set sy [lindex $args 1]
	    } else {
		return -code error "usage: transform scale s1 ?s2?"
	    }
	    set matrix [list [list $sx 0] [list 0 $sy] {0 0}]
	}
	skewx {
	    if {$len != 1} {
		return -code error "usage: transform skewx angle"
	    }
	    set sinPhi [expr sin([lindex $args 0])]
	    set matrix [list {1 0} [list $sinPhi 1] {0 0}]
	}
	skewy {
	    if {$len != 1} {
		return -code error "usage: transform skewy angle"
	    }
	    set sinPhi [expr sin([lindex $args 0])]
	    set matrix [list [list 1 $sinPhi] {0 1} {0 0}]
	}
	translate {
	    if {$len != 2} {
		return -code error "usage: transform translate x y"
	    }
	    set matrix [list {1 0} {0 1} [lrange $args 0 1]]
	}
	default {
	    return -code error "unrecognized transform command: \"$cmd\""
	}
    }
    return $matrix
}

# ::tkpath::coords --
# 
#       Helper for designing the path specification for some typical items.
#       These have SVG prototypes.
#       
# Arguments:
#       type         any of circle, ellipse, polygon, polyline, or rect.
#       coords       a list of the type specific coordinates
#       args         optional attributes, such as -rx and -ry for rect.
#       
# Results:
#       a transformation matrix

proc ::tkpath::coords {type coords args} {
    
    set len [llength $args]
    
    switch -- $type {
	circle {
	    if {[llength $coords] != 3} {
		return -code error "unrecognized circle coords \"$coords\""
	    }
	    foreach {cx cy r} $coords {break}
	    set path [list \
	      M $cx [expr $cy-$r] \
	      A $r $r 0 0 1 $cx [expr $cy+$r] \
	      A $r $r 0 0 1 $cx [expr $cy-$r] Z]
	}
	ellipse {
	    if {[llength $coords] != 4} {
		return -code error "unrecognized circle ellipse \"$coords\""
	    }
	    foreach {cx cy rx ry} $coords {break}
	    set path [list \
	      M $cx [expr $cy-$ry] \
	      A $rx $ry 0 0 1 $cx [expr $cy+$ry] \
	      A $rx $ry 0 0 1 $cx [expr $cy-$ry] Z]
	}
	polygon {
	    set path [concat M [lrange $coords 0 1] M [lrange $coords 2 end] Z]
	}
	polyline {
	    set path [concat M [lrange $coords 0 1] M [lrange $coords 2 end]]
	}
	rect {
	    if {[llength $coords] != 4} {
		return -code error "unrecognized rect coords \"$coords\""
	    }
	    foreach {x y width height} $coords {break}
	    if {$args == {}} {
		set path [list M $x $y h $width v $height h -$width z]
	    } else {
		set rx 0.0
		set ry 0.0
		foreach {key value} $args {
		    
		    switch -- $key {
			-rx - -ry {
			    set [string trimleft $key -] $value
			}
			default {
			    return -code error "unrecognized rect option $key"
			}
		    }
		}
		set x2 [expr $x+$width]
		set y2 [expr $y+$height]
		if {[expr 2*$rx] > $width} {
		    set rx [expr $width/2.0]
		}
		if {[expr 2*$ry] > $height} {
		    set ry [expr $height/2.0]
		}
		set dx [expr $width-2*$rx]
		set dy [expr $height-2*$ry]
		set path [list \
		  M [expr $x+$rx] $y \
		  h $dx \
		  a $rx $ry 0 0 1 $rx $ry \
		  v $dy \
		  a $rx $ry 0 0 1 -$rx $ry \
		  h [expr -1*$dx] \
		  a $rx $ry 0 0 1 -$rx -$ry \
		  v [expr -1*$dy] \
		  a $rx $ry 0 0 1 $rx -$ry Z]
	    }
	}
	default {
	    return -code error "unrecognized item type: \"$type\""
	}
    }
    return $path
}


