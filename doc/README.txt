                  
                             tkpath README
                             _____________

This package implements path drawing modelled after its SVG counterpart,
see http://www.w3.org/TR/SVG11/.

There are various differences compared to SVG. As a canvas item, it also
behaves a bit differently than an ordinary item.

SVG: It implements the complete syntax of the path elements d attribute with
one major difference: all separators must be whitespace, no commas, no
implicit assumptions; all instructions and numbers must form a tcl list.
The display attribute names are adapted to tcl conventions, see below.
Also, SVG is web oriented and therefore tolerates parameter errors to some
degree, while tk is a programming tool and typically generates errors
if paramters are wrong.

 o The options

    The options can be separated into a few groups depending on the nature
    of an item for which they apply. Not all are implemented.

    Fill (fillOptions):
        -fill color|gradientToken       this is either a usual tk color
                                        or the name of a gradient
        -fillopacity float (0,1)
        -fillrule nonzero|evenodd

    Stroke (strokeOptions):
        -stroke color
        -strokedasharray dashArray
        -strokelinecap 
        -strokelinejoin
        -strokemiterlimit float
        -strokeopacity float (0,1)
        -strokewidth float

    Generic (genericOptions):
        -matrix {{a b} {c d} {tx ty}}
        -state
        -style styleToken
        -tags tagList

    A matrix is specified by a double list as {{a b} {c d} {tx ty}}.
    There are utility functions to create a matrix using simpler transformations,
    such as rotation, translation etc.

    The styleToken can be a style created with tkpath::style. It's options
    take precedence over any other options set directly. This is how
    SVG works (bad?).

 o The path item

    The path specification must be a single list and not concateneted with
    the rest of the command:

    right:  .c create path {M 10 10 h 10 v 10 h -10 z} -fill blue
    wrong:  .c create path M 10 10 h 10 v 10 h -10 z -fill blue    ;# Error

    Furthermore, coordinates are pixel coordinates and nothing else.

    .c create path pathSpec ?fillOptions strokeOptions genericOptions?

    All path specifications are normalized initially to the fundamental atoms
    M, L, A, Q, and C, all upper case. When you use the canvas 'coords' command
    it is the normalized path spec that is returned.

    Visualize this as a pen which always has a current coordinate after
    the first M. Coordinates are floats:

      M x y   Put the pen on the paper at specified coordinate.
              Must be the first atom but can appear any time later.
      L x y   Draw a line to the given coordinate.
      H x     Draw a horizontal line to the given x coordinate.
      V y     Draw a vertical line to the given y coordinate.
      A rx ry phi largeArc sweep x y
              Draw an elliptical arc from the current point to (x, y). 
              The points are on an ellipse with x-radius rx and y-radius ry.
              The ellipse is rotated by phi degrees. If the arc is less than 
              180 degrees, largeArc is zero, else it is one. If the arc is to be
              drawn in cw direction, sweep is one, and zero for the ccw
              direction.
      Q x1 y1 x y
              Draw a qadratic Bezier curve from the current point to (x, y)
              using control point (x1, y1).
      T x y   Draw a qadratic Bezier curve from the current point to (x, y)
              The control point will be the reflection of the previous Q atoms
              control point.
      C x1 y1 x2 y2 x y
              Draw a cubic Bezier curve from the current point to (x, y)
              using control points (x1, y1) and (x2, y2).
      S x2 y2 x y
              Draw a cubic Bezier curve from the current point to (x, y), using
              (x2, y2) as the control point for this new endpoint. The first
              control point will be the reflection of the previous C atoms
              ending control point.
      Z       Close path by drawing from the current point to the preceeding M 
              point.

    You may use lower case characters for all atoms which then means that all
    coordinates where relevant, are interpreted as coordinates relative the
    current point.

 o The prect item

    This is a rectangle item with optionally rounded corners.
    Item specific options:

        -rx  corner x-radius, or if -ry not given it sets the uniform radius.
        -ry  corner y-radius

    .c create prect x1 y1 x2 y2 ?-rx -ry fillOptions strokeOptions genericOptions?

 o The circle item

   A plain circle item. Item specific options:

       -r  its radius; defaults to zero

   .c create circle cx cy ?-r fillOptions strokeOptions genericOptions?

 o The ellipse item

    An ellipse item. Item specific options:

        -rx  its x-radius
        -ry  its y-radius

    .c create ellipse cx cy ?-rx -ry fillOptions strokeOptions genericOptions?

 o The pline item

    Makes a single segment straight line.

    .c create pline x1 y1 x2 y2 ?strokeOptions genericOptions?

 o The polyline item

    Makes a multiple segment line with open ends.

    .c create polyline x1 y1 x2 y2 .... ?strokeOptions genericOptions?

 o The ppolygon item

    Makes a closed polygon.

    .c create ppolygon x1 y1 x2 y2 .... ?fillOptions strokeOptions genericOptions?

 o The pimage item

   This displays an image in the canvas anchored nw. If -width or -height is
   nonzero then the image is scaled to this size prior to any affine transform.

   .c create pimage x y ?-image -width -height?

 o The ptext item (cairo, quartz, gdi+)

   Displays text as expected. Note that the x coordinate marks the baseline
   of the text. Gradient fills unsupported so far. Not implemented in the
   Tk and GDI backends. Especially the font handling and settings will likely
   be developed further. Editing not implemented.
   
   .c create ptext x y ?-text string -textanchor start|middle|end?
       ?-fontfamily fontname -fontsize float?
       ?fillOptions strokeOptions genericOptions?

 o Antialiasing, if available, is controlled by the variable:
    ::tkpath::antialias
    Switch on with:
    set ::tkpath::antialias 1

 o The command ::tkpath::pixelalign returns how pixels are aligned to 
   coordinates. If 0 then pixels are between the integer coordinates, and if 1
   they are centered on the integer coordinates. If you draw lines and the
   graphics lib doesn't do pixel align you may have to draw to half integer
   coordinates to get sharp 1 pixel width lines.

 o Styles are created and configured using:

    ::tkpath::style cmd ?options?

        ::tkpath::style cget token option
            Returns the value of an option.

        ::tkpath::style configure token ?option? ?value option value...?
            Configures the object in the usual tcl way.

        ::tkpath::style create ?-key value ...?
            Creates a style object and returns its token.

        ::tkpath::style delete token
            Deletes the object.

        ::tkpath::style names
            Returns all existing tokens.

    The same options as for the item are supported with the exception of -style,
    -state, and -tags.


 o Gradients can be of two types, linear and radial. They are created and 
   configured using:

    ::tkpath::gradient cmd ?options?

        ::tkpath::gradient cget token option
            Returns the value of an option.

        ::tkpath::gradient configure token ?option? ?value option value...?
            Configures the object in the usual tcl way.

        ::tkpath::gradient create type ?-key value ...?
            Creates a linear gradient object with type any of linear or radial
            and returns its token.

        ::tkpath::gradient delete token
            Deletes the object.

        ::tkpath::gradient names
            Returns all existing tokens.

        ::tkpath::gradient type token
            Returns the type (linear|radial) of the gradient.

    The options for linear gradients are:
        -method pad|repeat|reflect    partial implementation; defaults to pad
        -stops {stopSpec ?stopSpec...?}
            where stopSpec is a list {offset color ?opacity?}.
            All offsets must be ordered and run from 0 to 1.
        -lineartransition {x1 y1 x2 y2}
            specifies the transtion vector relative the items bounding box.
            Depending on -units it gets interpreted differently.
            If -units is 'bbox' coordinates run from 0 to 1 and are relative
            the items bounding box. If -units is 'userspace' then they are
            defined in absolute coordinates but in the space of the items
            coordinate system. It defaults to {0 0 1 0}.
        -matrix {{a b} {c d} {tx ty}}
            sets a specific transformation for the gradient pattern only.
	    NB: not sure about the order transforms, see -units.
        -units bbox|userspace sets the units of the transition coordinates.
            See above. Defaults to bbox. Not implemented in the Tk and GDI 
            backends. 

    The options for radial gradients are the same as for linear gradients
    except that the -lineartransition is replaced by a -radialtransition:
       -radialtransition {cx cy ?r? ?fx fy?}
           specifies the transition circles relative the items bounding box
           and run from 0 to 1. They default to {0.5 0.5 0.5 0.5 0.5}.
           cx,cy is the center of the end circle and fx,fy the center of the
           start point.


 o In memory drawing surfaces (cairo, quartz, gdi+):

    ::tkpath::surface new width height

    creates an in memory drawing surface. Its format is platform dependent.
    It returns a token which is a new command.

    ::tkpath::surface names

    lists the existing surface tokens.

    The surface token commands are:

    $token copy imageName

    copies the surface to an existing image (photo) and returns the name of
    the image so you can do: 
    set image [$token copy [image create photo]]
    See Tk_PhotoPutBlock for how it affects the existing image.

    The boolean variable ::tkpath::premultiplyalpha controls how the copy
    action handles surfaces with the alpha component premultiplied. If 1 the
    copy process correctly handles any format with premultiplied alpha. This
    gets the highest quality for antialiasing and correct results for partial
    transparency. It is also slower. If 0 the alpha values are not remultiplied
    and the result is wrong for transparent regions, and gives poor antialiasing
    effects. But it is faster. The default is 1.

    $token create type coords ?options?

    draws the item of type to the surface. All item types and the corresponding
    options as described above are supported, except the canvas specific -tags
    and -state.

    $token destroy

    destroys surface.

    $token erase x y width height

    erases the indicated area to transparent.

    $token height 
    $token width

    returns height and width respectively.

    NB: gdi+ seems unable to produce antialiasing effects here but there seems
        to be no gdi+ specific way of drawing in memory bitmaps but had to call
        CreateDIBSection() which is a Win32 GDI API.


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
   coordinates using something like: [expr int($x) + 0.5]. If you fill and
   not stroke you may need to have integer coordinates to avoid antialiasing
   effects.

   WinXP: GDI+. On pre XP systems it should be possible to get the gdiplus.dll
   to make it work. Maybe there is also an issue with the MSCRT.LIB.

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

Copyright (c) 2005-2007  Mats Bengtsson

BSD style license.

