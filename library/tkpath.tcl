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
	    if {$len == 1} {
		set phi [lindex $args 0]
		set cosPhi [expr cos($phi)]
		set sinPhi [expr sin($phi)]
		set msinPhi [expr -1.0*$sinPhi]
		set matrix \
		  [list [list $cosPhi $sinPhi] [list $msinPhi $cosPhi] {0 0}]
	    } elseif {$len == 3} {
		set phi [lindex $args 0]
		set cx  [lindex $args 1]
		set cy  [lindex $args 2]
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


