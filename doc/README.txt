                  
                             tkpath README
                             _____________

This package implements path drawing modelled after its SVG counterpart,
see http://www.w3.org/TR/SVG11/.

The code is divided into two parts:

1) Generic path drawing which also implements the platform specific parts.

2) A path canvas item.

There are various differences compared to SVG. As a canvas item, it also
behaves a bit differently than an ordinary item.

SVG: It implements the complete syntax of the path elements d attribute with
one major difference: all separators must be whitespace, no commas, no
implicit assumptions; all instructions and numbers must form a tcl list.
The display attribute names are adapted to tcl conventions, see below.
Also, SVG is web oriented and therefore tolerates parameter errors to some
degree, while tk is a programming tool and typically generates errors
if paramters are wrong.

 o The canvas path item

    The path specification must be a single list and not concateneted with
    the rest of the command:

    right:  .c create path {M 10 10 h 10 v 10 h -10 z} -fill blue
    wrong:  .c create path M 10 10 h 10 v 10 h -10 z -fill blue    ;# Error

    Furthermore, coordinates are pixel coordinates and nothing else.

    Options for the path canvas item command. Not all are implemented:

	-fill color
	-fillgradient gradientToken
	-filloffset
	-fillopacity float (0,1)
	-fillrule nonzero|evenodd
	-fillstipple
	-matrix {{a b} {c d} {tx ty}}
	-state
	-stroke color
	-strokedasharray dashArray
	-strokelinecap 
	-strokelinejoin
	-strokemiterlimit float
	-strokeoffset
	-strokeopacity float (0,1)
	-strokestipple
	-strokewidth float
	-style styleToken
	-tags tagList

    A matrix is specified by a double list as {{a b} {c d} {tx ty}}.
    There are utility functions to create a matrix using simpler transformations,
    such as rotation, translation etc.

    The styleToken can be a style created with tkpath::style. It's options
    take precedence over any other options set directly. This is how
    SVG works (bad?).

    All path specifications are normalized initially to the fundamental atoms
    M, L, A, Q, and C, all upper case. When you use the canvas 'coords' command
    it is the normalized path spec that is returned.


 o Antialiasing, if available, is controlled by the variable:
    ::tkpath::antialias
    Switch on with:
    set ::tkpath::antialias 1


 o Styles are created and configured using:

    ::tkpath::style cmd ?options?

        ::tkpath:: style cget token option
	    Returns the value of an option.

        ::tkpath:: style configure token ?option? ?value option value...?
            Configures the object in the usual tcl way.

        ::tkpath:: style create ?-key value ...?
            Creates a linear gradient object and returns its token.

	::tkpath:: style delete token
	    Deletes the object.

	::tkpath:: style names
	    Returns all existing tokens.

    The same options as for the item are supported with the exception of -style,
    -state, and -tags.


 o Linear gradients are created and configured using:

    ::tkpath::lineargradient cmd ?options?

	::tkpath::lineargradient cget token option
	    Returns the value of an option.

	::tkpath::lineargradient configure token ?option? ?value option value...?
	    Configures the object in the usual tcl way.

	::tkpath::lineargradient create ?-key value ...?
	    Creates a linear gradient object and returns its token.

	::tkpath::lineargradient delete token
	    Deletes the object.

	::tkpath::lineargradient names
	    Returns all existing tokens.

    The options are:
	-method pad|repeat|reflect    partial implementation; defaults to pad
	-stops {stopSpec ?stopSpec...?}
	    where stopSpec is a list {offset color ?opacity?}.
	    All offsets must be ordered and run from 0 to 1.
	-transition {x1 y1 x2 y2}
	    specifies the transtion vector relative the items bounding box.
	    Coordinates run from 0 to 1. It defaults to {0 0 1 0}.


 o Helper function for making transformation matrices:

    ::tkpath::transform cmd ?args?

        ::tkpath::transform rotate angle ?centerX centerY?

	::tkpath::transform scale factorXY ?factorY?

	::tkpath::transform skewx angle

	::tkpath::transform skewy angle

	::tkpath::transform translate x y


 o Binaries and libraries:
 
   On some systems (CoreGraphics and cairo) lines that are not placed at the
   pixel centers, that is 8.0 12.4 etc., get an even number line width
   if not using antialiasing. For instance, a -strokewidth 1 results in
   a 2 pixel wide line. This is by design. If you want to be sure to get
   the exact width when not using antialiasing always pick pixel center
   coordinates using something like: [expr int($x) + 0.5]

   WinXP: GDI+. On pre XP systems it should be possible to get the gdiplus.dll
   to make it work.

   Win32: GDI. Features like opacity and antialiasing are missing here.


 o Known issues:

   - Any changes made to a style object or a gradient object is not directly
     noticable to a canvas item. Some kind of notifier is needed here.

   - The style and gradient objects should belong to the canvas widget itself,
     but that requires changes to the canvas code.

   - Avoid using the canvas scale command on paths containing arc instructions
     since an arc cannot generally be scaled and still be an arc.

 o Further documentation:

    - http://www.w3.org/TR/SVG11/

    - http://cairographics.org

Copyright (c) 2005  Mats Bengtsson

BSD style license.

