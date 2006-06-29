/*
 * tkCanvPath.c --
 *
 *	This file implements a path canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkCanvPathUtil.h"
#include "tkPathCopyTk.h"


int gDebugLevel = 2;
Tcl_Interp *gInterp;

#define PATH_DEBUG 0

/* Values for the PathItem's flag. */

enum {
    kPathItemNeedNewNormalizedPath                     = (1L << 0)
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PathItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Outline outline;		/* Outline structure */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    Tcl_Obj *pathObjPtr;	/* The object containing the path definition. */
    int pathLen;
    Tcl_Obj *normPathObjPtr;/* The object containing the normalized path. */
    PathAtom *atomPtr;
    PathRect bareBbox;		/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    PathRect totalBbox;		/* Bounding box including stroke.
                             * Untransformed coordinates. */
    int maxNumSegments;		/* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    long flags;				/* Various flags, see enum. */
} PathItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePathBbox(Tk_Canvas canvas, PathItem *pathPtr);
static int		ConfigurePath(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePath(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable dst,
                        int x, int y, int width, int height);
static int		PathCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PathToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *areaPtr);
static double	PathToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PathToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);


/* Support functions. */

static int		GetSubpathMaxNumSegments(PathAtom *atomPtr);


extern int 		LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                        int objc, Tcl_Obj* CONST objv[]);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(PathItem),
    PATH_CONFIG_SPEC_STYLE_MATRIX(PathItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(PathItem),
    PATH_CONFIG_SPEC_CORE(PathItem),
    PATH_END_CONFIG_SPEC
};

/*
 * The structures below defines the 'path' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPathType = {
    "path",							/* name */
    sizeof(PathItem),				/* itemSize */
    CreatePath,						/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePath,					/* configureProc */
    PathCoords,						/* coordProc */
    DeletePath,						/* deleteProc */
    DisplayPath,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PathToPoint,					/* pointProc */
    PathToArea,						/* areaProc */
    PathToPostscript,				/* postscriptProc */
    ScalePath,						/* scaleProc */
    TranslatePath,					/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};

/* This one seems missing. */
#if 0
static Tcl_Interp *
Tk_CanvasInterp(Tk_Canvas canvas)
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    return canvasPtr->interp;
}
#endif


void
DebugPrintf(Tcl_Interp *interp, int level, char *fmt, ...)
{
	va_list		args;
	char		tmpstr[256];
	
	if (level > gDebugLevel) {
		return;
	}
	va_start( args, fmt );
	vsprintf( tmpstr, fmt, args );	
    Tcl_VarEval( interp, "puts \"", tmpstr, "\"", (char *) NULL );
	va_end (args );
}

#if 0
static int
IsPathRectEmpty(PathRect *r)
{
    if ((r->x2 > r->x1) && (r->y2 > r->y1)) {
        return 0;
    } else {
        return 1;
    }
}
#endif

/* Be sure rect is not empty (see above) before doing this. */
static void
NormalizePathRect(PathRect *r)
{
    double min, max;

    min = MIN(r->x1, r->x2);
    max = MAX(r->x1, r->x2);
    r->x1 = min;
    r->x2 = max;
    min = MIN(r->y1, r->y2);
    max = MAX(r->y1, r->y2);
    r->y1 = min;
    r->y2 = max;
}

/* 
 +++ This starts the canvas item part +++ 
 */

/*
 *--------------------------------------------------------------
 *
 * CreatePath --
 *
 *		This procedure is invoked to create a new line item in
 *		a canvas.
 *
 * Results:
 *		A standard Tcl return value.  If an error occurred in
 *		creating the item, then an error message is left in
 *		the interp's result;  in this case itemPtr is left uninitialized,
 *		so it can be safely freed by the caller.
 *
 * Side effects:
 *		A new line item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreatePath(
        Tcl_Interp *interp, 	/* Used for error reporting. */
        Tk_Canvas canvas, 	/* Canvas containing item. */
        Tk_Item *itemPtr, 	/* Item to create. */
        int objc,		/* Number of elements in objv.  */
        Tcl_Obj *CONST objv[])	/* Arguments describing the item. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }
    gInterp = interp;

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */

    Tk_CreateOutline(&(pathPtr->outline));
    Tk_CreatePathStyle(&(pathPtr->style));
    pathPtr->canvas = canvas;
    pathPtr->pathObjPtr = NULL;
    pathPtr->pathLen = 0;
    pathPtr->normPathObjPtr = NULL;
    pathPtr->styleName = NULL;
    pathPtr->atomPtr = NULL;
    pathPtr->bareBbox = NewEmptyPathRect();
    pathPtr->totalBbox = NewEmptyPathRect();
    pathPtr->maxNumSegments = 0;
    pathPtr->flags = 0L;
    
    /* Forces a computation of the normalized path in PathCoords. */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /*
     * The first argument must be the path definition list.
     */

    if (PathCoords(interp, canvas, itemPtr, 1, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePath(interp, canvas, itemPtr, objc-1, objv+1, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    DeletePath(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * PathCoords --
 *
 *		This procedure is invoked to process the "coords" widget
 *		command on lines.  See the user documentation for details
 *		on what it does.
 *
 * Results:
 *		Returns TCL_OK or TCL_ERROR, and sets the interp's result.
 *
 * Side effects:
 *		The coordinates for the given item may be changed.
 *
 *--------------------------------------------------------------
 */

static int
PathCoords(
    Tcl_Interp *interp,			/* Used for error reporting. */
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item whose coordinates are to be
                                 * read or modified. */
    int objc,					/*  */
    Tcl_Obj *CONST objv[])		/*  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = NULL;
    int result, len;
    
    if (objc == 0) {
        /* @@@ We have an option here if to return the normalized or original path. */
        //Tcl_SetObjResult(interp, pathPtr->pathObjPtr);
        
        /* We may need to recompute the normalized path from the atoms. */
        if (pathPtr->flags & kPathItemNeedNewNormalizedPath) {
            if (pathPtr->normPathObjPtr != NULL) {
                Tcl_DecrRefCount(pathPtr->normPathObjPtr);
            }
            TkPathNormalize(interp, pathPtr->atomPtr, &(pathPtr->normPathObjPtr));
        }
        Tcl_SetObjResult(interp, pathPtr->normPathObjPtr);
        return TCL_OK;
    } else if (objc == 1) {
        result = TkPathParseToAtoms(interp, objv[0], &atomPtr, &len);
        if (result == TCL_OK) {
        
            /* Free any old atoms. */
            if (pathPtr->atomPtr != NULL) {
                TkPathFreeAtoms(pathPtr->atomPtr);
            }
            pathPtr->atomPtr = atomPtr;
            pathPtr->pathLen = len;
            pathPtr->pathObjPtr = objv[0];
            Tcl_IncrRefCount(pathPtr->pathObjPtr);
            ComputePathBbox(canvas, pathPtr);
            pathPtr->maxNumSegments = GetSubpathMaxNumSegments(atomPtr);
        }
        return result;
    } else {
        Tcl_WrongNumArgs(interp, 0, objv, "pathName coords id ?pathSpec?");
        return TCL_ERROR;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ConfigurePath --
 *
 *		This procedure is invoked to configure various aspects
 *		of a line item such as its background color.
 *
 * Results:
 *		A standard Tcl result code.  If an error occurs, then
 *		an error message is left in the interp's result.
 *
 * Side effects:
 *		Configuration information, such as colors and stipple
 *		patterns, may be set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigurePath(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_Canvas canvas,		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr,		/* Line item to reconfigure. */
    int objc,			/* Number of elements in objv.  */
    Tcl_Obj *CONST objv[],	/* Arguments describing things to configure. */
    int flags)			/* Flags to pass to Tk_ConfigureWidget. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathStyle *style = &(pathPtr->style);
    unsigned long mask;
    Tk_Window tkwin;
    Tk_State state;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) pathPtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    style->strokeOpacity = MAX(0.0, MIN(1.0, style->strokeOpacity));
    style->fillOpacity   = MAX(0.0, MIN(1.0, style->fillOpacity));
    
    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (pathPtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, style, pathPtr->styleName, 0);
    } 

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    state = itemPtr->state;

    /*
    if (pathPtr->outline.activeWidth > pathPtr->outline.width ||
	    pathPtr->outline.activeDash.number != 0 ||
	    pathPtr->outline.activeColor != NULL ||
	    pathPtr->outline.activeStipple != None) {
	itemPtr->redraw_flags |= TK_ITEM_STATE_DEPENDANT;
    } else {
	itemPtr->redraw_flags &= ~TK_ITEM_STATE_DEPENDANT;
    }
    */
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        //ComputePathBbox(canvas, pathPtr);
        return TCL_OK;
    }
    
    /* @@@ Not sure if GC's should be used at all! 
     *     Used in TkDraw!
     */
    mask = Tk_ConfigPathStylesGC(canvas, itemPtr, style);

    /*
     * Recompute bounding box for path.
     */
    ComputePathBbox(canvas, pathPtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeletePath --
 *
 *		This procedure is called to clean up the data structure
 *		associated with a line item.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Resources associated with itemPtr are released.
 *
 *--------------------------------------------------------------
 */

static void
DeletePath(
    Tk_Canvas canvas,			/* Info about overall canvas widget. */
    Tk_Item *itemPtr,			/* Item that is being deleted. */
    Display *display)			/* Display containing window for
                                 * canvas. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;

    Tk_DeleteOutline(display, &(pathPtr->outline));
    if (pathPtr->pathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->pathObjPtr);
    }
    if (pathPtr->normPathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->normPathObjPtr);
    }
    if (pathPtr->atomPtr != NULL) {
        TkPathFreeAtoms(pathPtr->atomPtr);
        pathPtr->atomPtr = NULL;
    }
    if (pathPtr->styleName != NULL) {
        ckfree(pathPtr->styleName);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputePathBbox --
 *
 *		This procedure is invoked to compute the bounding box of
 *		all the pixels that may be drawn as part of a path.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The fields x1, y1, x2, and y2 are updated in the header
 *		for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputePathBbox(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    PathItem *pathPtr)			/* Item whose bbox is to be
                                 * recomputed. */
{
    Tk_PathStyle *stylePtr = &(pathPtr->style);
    Tk_State state = pathPtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (pathPtr->pathObjPtr == NULL || (pathPtr->pathLen < 4) || (state == TK_STATE_HIDDEN)) {
        pathPtr->header.x1 = pathPtr->header.x2 =
        pathPtr->header.y1 = pathPtr->header.y2 = -1;
        return;
    }
    
    /*
     * Get an approximation of the path's bounding box
     * assuming zero width outline (stroke).
     */
    pathPtr->bareBbox = GetGenericBarePathBbox(pathPtr->atomPtr);

    pathPtr->totalBbox = GetGenericPathTotalBboxFromBare(pathPtr->atomPtr,
            stylePtr, &(pathPtr->bareBbox));
    SetGenericPathHeaderBbox(&(pathPtr->header), stylePtr->matrixPtr, &(pathPtr->totalBbox));
}

/*
 *--------------------------------------------------------------
 *
 * DisplayPath --
 *
 *		This procedure is invoked to draw a line item in a given
 *		drawable.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		ItemPtr is drawn in drawable using the transformation
 *		information in canvas.
 *
 *--------------------------------------------------------------
 */

static void
DisplayPath(
    Tk_Canvas canvas,		/* Canvas that contains item. */
    Tk_Item *itemPtr,		/* Item to be displayed. */
    Display *display,		/* Display on which to draw item. */
    Drawable drawable,		/* Pixmap or window in which to draw
                             * item. */
    int x, int y, 			/* Describes region of canvas that */
    int width, int height)	/* must be redisplayed (not used). */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    if (pathPtr->pathLen > 2) {
        TkPathDrawPath(Tk_CanvasTkwin(canvas), drawable, pathPtr->atomPtr, &(pathPtr->style),
                &m, &(pathPtr->bareBbox));
    }
}

/*
 *--------------------------------------------------------------
 *
 * PathToPoint --
 *
 *		Computes the distance from a given point to a given
 *		line, in canvas units.
 *
 * Results:
 *		The return value is 0 if the point whose x and y coordinates
 *		are pointPtr[0] and pointPtr[1] is inside the line.  If the
 *		point isn't inside the line then the return value is the
 *		distance from the point to the line.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static double
PathToPoint(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against point. */
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
    PathItem 		*pathPtr = (PathItem *) itemPtr;
    PathAtom		*atomPtr = pathPtr->atomPtr;
    Tk_PathStyle 	*stylePtr = &(pathPtr->style);

    return GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            pathPtr->maxNumSegments, pointPtr);
}

/**********************************/

double
TkLineToPoint2(end1Ptr, end2Ptr, pointPtr)
    double end1Ptr[2];		/* Coordinates of first end-point of line. */
    double end2Ptr[2];		/* Coordinates of second end-point of line. */
    double pointPtr[2];		/* Points to coords for point. */
{
    double dx, dy, a2, b2, c2;

    /*
     * Compute the point on the line that is closest to the
     * point. Use Pythagoras!
     * Notation:
     *	a = distance between end1 and end2
     * 	b = distance between end1 and point
     *	c = distance between end2 and point
     *
     *   point
     *    |\
     *    | \
     *  b |  \ c
     *    |   \
     *    |----\
     * end1  a  end2
     *
     * If angle between a and b is 90 degrees: c2 = a2 + b2
     * If larger then c2 > a2 + b2 and end1 is closest to point
     * Similar for end2 with b and c interchanged.
     */
     
    dx = end1Ptr[0] - end2Ptr[0];
    dy = end1Ptr[1] - end2Ptr[1];
    a2 = dx*dx + dy*dy;

    dx = end1Ptr[0] - pointPtr[0];
    dy = end1Ptr[1] - pointPtr[1];
    b2 = dx*dx + dy*dy;

    dx = end2Ptr[0] - pointPtr[0];
    dy = end2Ptr[1] - pointPtr[1];
    c2 = dx*dx + dy*dy;
    
    if (c2 >= a2 + b2) {
        return sqrt(b2);
    } else if (b2 >= a2 + c2) {
        return sqrt(c2);
    } else {
        double delta;
        
        /* 
         * The closest point is found at the point between end1 and end2
         * that is perp to point. delta is the distance from end1 along
         * that line which is closest to point.
         */
        delta = (a2 + b2 - c2)/(2.0*sqrt(a2));
        return sqrt(MAX(0.0, b2 - delta*delta));
    }
}

/* 
 * Get maximum number of segments needed to describe path. 
 * Needed to see if we can use static space or need to allocate more.
 */

static int
GetArcNumSegments(double currentX, double currentY, ArcAtom *arc)
{
    int result;
    int ntheta, nlength;
    int numSteps;			/* Number of curve points to
					 * generate.  */
    double cx, cy, rx, ry;
    double theta1, dtheta;

    result = EndpointToCentralArcParameters(
            currentX, currentY,
            arc->x, arc->y, arc->radX, arc->radY, 
            DEGREES_TO_RADIANS * arc->angle, 
            arc->largeArcFlag, arc->sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
    if (result == kPathArcLine) {
        return 2;
    } else if (result == kPathArcSkip) {
        return 0;
    }

    /* Estimate the number of steps needed. 
     * Max 10 degrees or length 50.
     */
    ntheta = (int) (dtheta/5.0 + 0.5);
    nlength = (int) (0.5*(rx + ry)*dtheta/50 + 0.5);
    numSteps = MAX(4, MAX(ntheta, nlength));;
    return numSteps;
}

static int
GetSubpathMaxNumSegments(PathAtom *atomPtr)
{
    int			num;
    int 		maxNumSegments;
    double 		currentX = 0.0, currentY = 0.0;
    double 		startX = 0.0, startY = 0.0;
    MoveToAtom 	*move;
    LineToAtom 	*line;
    ArcAtom 	*arc;
    QuadBezierAtom *quad;
    CurveToAtom *curve;
    
    num = 0;
    maxNumSegments = 0;
    
    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
                num = 1;
                currentX = move->x;
                currentY = move->y;
                startX = currentX;
                startY = currentY;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                num++;
                currentX = line->x;
                currentY = line->y;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                num += GetArcNumSegments(currentX, currentY, arc);
                currentX = arc->x;
                currentY = arc->y;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                num += kPathNumSegmentsQuadBezier;
                currentX = quad->anchorX;
                currentY = quad->anchorY;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                num += kPathNumSegmentsCurveTo;
                currentX = curve->anchorX;
                currentY = curve->anchorY;
                break;
            }
            case PATH_ATOM_Z: {
                num++;
                currentX = startX;
                currentY = startY;
                break;
            }
            case PATH_ATOM_ELLIPSE: {
                /* Empty. */
                break;
            }
        }
        if (num > maxNumSegments) {
            maxNumSegments = num;
        }
        atomPtr = atomPtr->nextPtr;
    }
    return maxNumSegments;
}

/*
 *--------------------------------------------------------------
 *
 * PathToArea --
 *
 *		This procedure is called to determine whether an item
 *		lies entirely inside, entirely outside, or overlapping
 *		a given rectangular area.
 *	
 *		Each subpath is treated in turn. Generate straight line
 *		segments for each subpath and treat it as a polygon.
 *
 * Results:
 *		-1 is returned if the item is entirely outside the
 *		area, 0 if it overlaps, and 1 if it is entirely
 *		inside the given area.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
PathToArea(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against line. */
    double *areaPtr)		/* Pointer to array of four coordinates
                             * (x1, y1, x2, y2) describing rectangular
                             * area.  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    
    return GenericPathToArea(canvas, itemPtr, &(pathPtr->style), 
            pathPtr->atomPtr, pathPtr->maxNumSegments, areaPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ScalePath --
 *
 *		This procedure is invoked to rescale a line item.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The line referred to by itemPtr is rescaled so that the
 *		following transformation is applied to all point
 *		coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScalePath(
    Tk_Canvas canvas,			/* Canvas containing line. */
    Tk_Item *itemPtr,			/* Line to be scaled. */
    double originX, double originY,	/* Origin about which to scale rect. */
    double scaleX,			/* Amount to scale in X direction. */
    double scaleY)			/* Amount to scale in Y direction. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;
    PathRect r;
    
    /* @@@ We shouldn't relly do this since it doesn't work well with affine transforms! 
           I think it is the arc element that is the problem. */

    ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
    
    /* 
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just scale the bbox'es as well. */
    r = pathPtr->bareBbox;
    r.x1 = originX + scaleX*(r.x1 - originX);
    r.y1 = originY + scaleX*(r.y1 - originY);
    r.x2 = originX + scaleX*(r.x2 - originX);
    r.y2 = originY + scaleX*(r.y2 - originY);
    NormalizePathRect(&r);
    pathPtr->bareBbox = r;
    
    r = pathPtr->totalBbox;
    r.x1 = originX + scaleX*(r.x1 - originX);
    r.y1 = originY + scaleX*(r.y1 - originY);
    r.x2 = originX + scaleX*(r.x2 - originX);
    r.y2 = originY + scaleX*(r.y2 - originY);
    NormalizePathRect(&r);
    pathPtr->bareBbox = r;
}

/*
 *--------------------------------------------------------------
 *
 * TranslatePath --
 *
 *		This procedure is called to move a path by a given amount.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The position of the line is offset by (xDelta, yDelta), and
 *		the bounding box is updated in the generic part of the item
 *		structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslatePath(
    Tk_Canvas canvas, 			/* Canvas containing item. */
    Tk_Item *itemPtr, 			/* Item that is being moved. */
    double deltaX,				/* Amount by which item is to be */
    double deltaY)              /* moved. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;

    TranslatePathAtoms(atomPtr, deltaX, deltaY);
    
    /* 
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(pathPtr->bareBbox), deltaX, deltaY);
    TranslatePathRect(&(pathPtr->totalBbox), deltaX, deltaY);

    pathPtr->header.x1 = (int) pathPtr->totalBbox.x1;
    pathPtr->header.x2 = (int) pathPtr->totalBbox.x2;
    pathPtr->header.y1 = (int) pathPtr->totalBbox.y1;
    pathPtr->header.y2 = (int) pathPtr->totalBbox.y2;
}

/*
 *--------------------------------------------------------------
 *
 * PathToPostscript --
 *
 *		This procedure is called to generate Postscript for
 *		path items.
 *
 * Results:
 *		The return value is a standard Tcl result.  If an error
 *		occurs in generating Postscript then an error message is
 *		left in the interp's result, replacing whatever used
 *		to be there.  If no error occurs, then Postscript for the
 *		item is appended to the result.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
PathToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
    return TCL_ERROR;
}


