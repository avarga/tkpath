/*
 * tkCanvPathUtil.c --
 *
 *	This file implements a path canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2006  Mats Bengtsson
 *
 * $Id$
 */

#include "tkCanvPathUtil.h"

/*
 * For wider strokes we must make a more detailed analysis
 * when doing hit tests and area tests.
 */
static double kPathStrokeThicknessLimit = 4.0;

#define MAX_NUM_STATIC_SEGMENTS  2000
/* @@@ Should this be moved inside the function instead? */
static double staticSpace[2*MAX_NUM_STATIC_SEGMENTS];


static int		GetSubpathMaxNumSegments(PathAtom *atomPtr);
static void		MakeSubPathSegments(PathAtom **atomPtrPtr, double *polyPtr, 
                        int *numPointsPtr, int *numStrokesPtr, TMatrix *matrixPtr);
static int		SubPathToArea(Tk_PathStyle *stylePtr, double *polyPtr, int numPoints,
                        int	numStrokes,	double *rectPtr, int inside);
static int		AddArcSegments(TMatrix *matrixPtr, double current[2], ArcAtom *arc,
                        double *coordPtr);
static int		AddQuadBezierSegments(TMatrix *matrixPtr, double current[2],		
                        QuadBezierAtom *quad, double *coordPtr);
static int		AddCurveToSegments(TMatrix *matrixPtr, double current[2],			
                        CurveToAtom *curve, double *coordPtr);


/*
 *--------------------------------------------------------------
 *
 * Tk_ConfigPathStylesGC
 *
 *		This procedure should be called in the canvas path object
 *		during the configure command. The strokeGC and fillGC
 *		are updated according to the information in the stylePtr.
 *
 *		@@@ Only for Tk drawing?
 *
 * Results:
 *		The return-value is a mask, indicating which
 *		elements of gcValues have been updated.
 *
 * Side effects:
 *		GC information in strokeGC and fillGC is updated.
 *
 *--------------------------------------------------------------
 */

int
Tk_ConfigPathStylesGC(Tk_Canvas canvas, Tk_Item *itemPtr, Tk_PathStyle *stylePtr)
{
    XGCValues gcValues;
    GC newGC;
    unsigned long maskStroke, maskFill;
    Tk_Window tkwin;

    tkwin = Tk_CanvasTkwin(canvas);

    /* 
     * Handle the strokeGC and fillGC used only (?) for Tk drawing. 
     */
    maskStroke = Tk_ConfigStrokePathStyleGC(&gcValues, canvas, itemPtr, stylePtr);
    if (maskStroke) {
        newGC = Tk_GetGC(tkwin, maskStroke, &gcValues);
    } else {
        newGC = None;
    }
    if (stylePtr->strokeGC != None) {
        Tk_FreeGC(Tk_Display(tkwin), stylePtr->strokeGC);
    }
    stylePtr->strokeGC = newGC;

    maskFill = Tk_ConfigFillPathStyleGC(&gcValues, canvas, itemPtr, stylePtr);
    if (maskFill) {
        newGC = Tk_GetGC(tkwin, maskFill, &gcValues);
    } else {
        newGC = None;
    }
    if (stylePtr->fillGC != None) {
        Tk_FreeGC(Tk_Display(tkwin), stylePtr->fillGC);
    }
    stylePtr->fillGC = newGC;

    return maskStroke | maskFill;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_ConfigStrokePathStyleGC
 *
 *		This procedure should be called in the canvas object
 *		during the configure command. The graphics context
 *		description in gcValues is updated according to the
 *		information in the dash structure, as far as possible.
 *
 * Results:
 *		The return-value is a mask, indicating which
 *		elements of gcValues have been updated.
 *		0 means there is no outline.
 *
 * Side effects:
 *		GC information in gcValues is updated.
 *
 *--------------------------------------------------------------
 */

/* @@@ Note: this is likely to be incomplete! */

int 
Tk_ConfigStrokePathStyleGC(
        XGCValues *gcValues, Tk_Canvas canvas,
        Tk_Item *item, Tk_PathStyle *stylePtr)
{
    int 	mask = 0;
    double 	width;
    Tk_Dash *dash;
    XColor 	*color;
    Pixmap 	stipple;
    Tk_State state = item->state;

    if (stylePtr->strokeWidth < 0.0) {
        stylePtr->strokeWidth = 0.0;
    }
    if (state == TK_STATE_HIDDEN) {
        return 0;
    }

    width = stylePtr->strokeWidth;
    if (width < 1.0) {
        width = 1.0;
    }
    dash = &(stylePtr->dash);
    color = stylePtr->strokeColor;
    stipple = stylePtr->strokeStipple;
    if (state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (color == NULL) {
        return 0;
    }

    gcValues->line_width = (int) (width + 0.5);
    if (color != NULL) {
        gcValues->foreground = color->pixel;
        mask = GCForeground|GCLineWidth;
        if (stipple != None) {
            gcValues->stipple = stipple;
            gcValues->fill_style = FillStippled;
            mask |= GCStipple|GCFillStyle;
        }
    }
    if (mask && (dash->number != 0)) {
        gcValues->line_style = LineOnOffDash;
        gcValues->dash_offset = stylePtr->offset;
        if (dash->number >= 2) {
            gcValues->dashes = 4;
        } else if (dash->number > 0) {
            gcValues->dashes = dash->pattern.array[0];
        } else {
            gcValues->dashes = (char) (4 * width);
        }
        mask |= GCLineStyle|GCDashList|GCDashOffset;
    }
    gcValues->cap_style = stylePtr->capStyle;
    mask |= GCCapStyle;
    gcValues->join_style = stylePtr->joinStyle;
    mask |= GCJoinStyle;
    return mask;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_ConfigFillPathStyleGC
 *
 *		This procedure should be called in the canvas object
 *		during the configure command. The graphics context
 *		description in gcValues is updated according to the
 *		information in the dash structure, as far as possible.
 *
 * Results:
 *		The return-value is a mask, indicating which
 *		elements of gcValues have been updated.
 *		0 means there is no outline.
 *
 * Side effects:
 *		GC information in gcValues is updated.
 *
 *--------------------------------------------------------------
 */

/* @@@ Note: this is likely to be incomplete! */

int 
Tk_ConfigFillPathStyleGC(XGCValues *gcValues, Tk_Canvas canvas,
        Tk_Item *item, Tk_PathStyle *stylePtr)
{
    int 	mask = 0;
    XColor 	*color;
    Pixmap 	stipple;

    color = stylePtr->fillColor;
    stipple = stylePtr->fillStipple;

    if (color != NULL) {
        gcValues->foreground = color->pixel;
        mask = GCForeground;
        if (stipple != None) {
            gcValues->stipple = stipple;
            gcValues->fill_style = FillStippled;
            mask |= GCStipple|GCFillStyle;
        }
    }
    return mask;
}

/*
 *--------------------------------------------------------------
 *
 * CoordsForRectangularItems --
 *
 *		Used as coordProc for items that have rectangular coords.
 *
 * Results:
 *		Standard tcl result.
 *
 * Side effects:
 *		May store new coords in rectPtr.
 *
 *--------------------------------------------------------------
 */

int		
CoordsForRectangularItems(
        Tcl_Interp *interp, 
        Tk_Canvas canvas, 
        PathRect *rectPtr, 		/* Sets or gets the box here. */
        int objc, 
        Tcl_Obj *CONST objv[])
{
    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewObj();
        Tcl_Obj *subobj = Tcl_NewDoubleObj(rectPtr->x1);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(rectPtr->y1);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(rectPtr->x2);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(rectPtr->y2);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        Tcl_SetObjResult(interp, obj);
    } else if ((objc == 1) || (objc == 4)) {
        double x1, y1, x2, y2;
        
        if (objc==1) {
            if (Tcl_ListObjGetElements(interp, objv[0], &objc,
                    (Tcl_Obj ***) &objv) != TCL_OK) {
                return TCL_ERROR;
            } else if (objc != 4) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 4", -1));
                return TCL_ERROR;
            }
        }
        if ((Tk_CanvasGetCoordFromObj(interp, canvas, objv[0], &x1) != TCL_OK)
            || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[1], &y1) != TCL_OK)
            || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[2], &x2) != TCL_OK)
            || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[3], &y2) != TCL_OK)) {
            return TCL_ERROR;
        }
        
        /*
         * Get an approximation of the path's bounding box
         * assuming zero width outline (stroke).
         * Normalize the corners!
         */
        rectPtr->x1 = MIN(x1, x2);
        rectPtr->y1 = MIN(y1, y2);
        rectPtr->x2 = MAX(x1, x2);
        rectPtr->y2 = MAX(y1, y2);
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 4", -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * CoordsForPolygonline --
 *
 *		Used as coordProc for polyline and polygon items.
 *
 * Results:
 *		Standard tcl result.
 *
 * Side effects:
 *		May store new atoms in atomPtrPtr and max number of points
 *		in lenPtr.
 *
 *--------------------------------------------------------------
 */

int		
CoordsForPolygonline(
        Tcl_Interp *interp, 
        Tk_Canvas canvas, 
        int closed,				/* Polyline (0) or polygon (1) */
        int objc, 
        Tcl_Obj *CONST objv[],
        PathAtom **atomPtrPtr,
        int *lenPtr)
{
    PathAtom *atomPtr = *atomPtrPtr;

    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
        
        while (atomPtr != NULL) {
            switch (atomPtr->type) {
                case PATH_ATOM_M: { 
                    MoveToAtom *move = (MoveToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->x));
                    break;
                }
                case PATH_ATOM_L: {
                    LineToAtom *line = (LineToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->x));
                    break;
                }
                case PATH_ATOM_Z: {
                
                    break;
                }
                default: {
                    /* empty */
                }
            }
            atomPtr = atomPtr->nextPtr;
        }
        Tcl_SetObjResult(interp, obj);
        return TCL_OK;
    }
    if (objc == 1) {
        if (Tcl_ListObjGetElements(interp, objv[0], &objc,
            (Tcl_Obj ***) &objv) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    if (objc & 1) {
        char buf[64 + TCL_INTEGER_SPACE];
        sprintf(buf, "wrong # coordinates: expected an even number, got %d", objc);
        Tcl_SetResult(interp, buf, TCL_VOLATILE);
        return TCL_ERROR;
    } else if (objc < 4) {
        char buf[64 + TCL_INTEGER_SPACE];
        sprintf(buf, "wrong # coordinates: expected at least 4, got %d", objc);
        Tcl_SetResult(interp, buf, TCL_VOLATILE);
        return TCL_ERROR;
    } else {
        int 	i;
        double	x, y;
        double	firstX, firstY;
        PathAtom *firstAtomPtr = NULL;
    
        /*
        * Free any old stuff.
        */
        if (atomPtr != NULL) {
            TkPathFreeAtoms(atomPtr);
            atomPtr = NULL;
        }
        for (i = 0; i < objc; i += 2) {
            if (Tk_CanvasGetCoordFromObj(interp, canvas, objv[i], &x) != TCL_OK) {
                /* @@@ error recovery? */
                return TCL_ERROR;
            }
            if (Tk_CanvasGetCoordFromObj(interp, canvas, objv[i+1], &y) != TCL_OK) {
                return TCL_ERROR;
            }
            if (i == 0) {
                firstX = x;
                firstY = y;
                atomPtr = NewMoveToAtom(x, y);
                firstAtomPtr = atomPtr;
            } else {
                atomPtr->nextPtr = NewLineToAtom(x, y);
                atomPtr = atomPtr->nextPtr;
            }
        }
        if (closed) {
            atomPtr->nextPtr = NewCloseAtom(firstX, firstY);
        }
        *atomPtrPtr = firstAtomPtr;
        *lenPtr = i/2 + 2;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GetBareArcBbox
 *
 *		Gets an overestimate of the bounding box rectangle of
 * 		an arc defined using central parametrization assuming
 *		zero stroke width.
 * 		Untransformed coordinates!
 *		Note: 1) all angles clockwise direction!
 *	    	  2) all angles in radians.
 *
 * Results:
 *		A PathRect.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static PathRect
GetBareArcBbox(double cx, double cy, double rx, double ry,
        double theta1, double dtheta, double phi)
{
    PathRect r = {1.0e36, 1.0e36, -1.0e36, -1.0e36};	/* Empty rect. */
    double start, extent, stop, stop2PI;
    double cosStart, sinStart, cosStop, sinStop;
    
    /* Keep 0 <= start, extent < 2pi 
     * and 0 <= stop < 4pi */
    if (dtheta >= 0.0) {
        start = theta1;
        extent = dtheta;
    } else {
        start = theta1 + dtheta;
        extent = -1.0*dtheta;
    }
    if (start < 0.0) {
        start += 2.0*PI;
        if (start < 0.0) {
            start += 2.0*PI;
        }
    }
    if (start >= 2.0*PI) {
        start -= 2.0*PI;
    }
    stop = start + extent;
    stop2PI = stop - 2.0*PI;
    cosStart = cos(start);
    sinStart = sin(start);
    cosStop = cos(stop);
    sinStop = sin(stop);
    
    /*
     * Compute bbox for phi = 0.
     * Put everything at (0,0) and shift to (cx,cy) at the end.
     * Look for extreme points of arc:
     * 	1) start and stop points
     *	2) any intersections of x and y axes
     * Count both first and second "turns".
     */
                
    IncludePointInRect(&r, rx*cosStart, ry*sinStart);
    IncludePointInRect(&r, rx*cosStop,  ry*sinStop);
    if (((start < PI/2.0) && (stop > PI/2.0)) || (stop2PI > PI/2.0)) {
        IncludePointInRect(&r, 0.0, ry);
    }
    if (((start < PI) && (stop > PI)) || (stop2PI > PI)) {
        IncludePointInRect(&r, -rx, 0.0);
    }
    if (((start < 3.0*PI/2.0) && (stop > 3.0*PI/2.0)) || (stop2PI > 3.0*PI/2.0)) {
        IncludePointInRect(&r, 0.0, -ry);
    }
    if (stop > 2.0*PI) {
        IncludePointInRect(&r, rx, 0.0);
    }
    
    /*
     * Rotate the bbox above to get an overestimate of extremas.
     */
    if (fabs(phi) > 1e-6) {
        double cosPhi, sinPhi;
        double x, y;
        PathRect rrot = {1.0e36, 1.0e36, -1.0e36, -1.0e36};
        
        cosPhi = cos(phi);
        sinPhi = sin(phi);
        x = r.x1*cosPhi - r.y1*sinPhi;
        y = r.x1*sinPhi + r.y1*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x2*cosPhi - r.y1*sinPhi;
        y = r.x2*sinPhi + r.y1*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x1*cosPhi - r.y2*sinPhi;
        y = r.x1*sinPhi + r.y2*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x2*cosPhi - r.y2*sinPhi;
        y = r.x2*sinPhi + r.y2*cosPhi;
        IncludePointInRect(&rrot, x, y);

        r = rrot;
    }
    
    /* Shift rect to arc center. */
    r.x1 += cx;
    r.y1 += cy;
    r.x2 += cx;
    r.y2 += cy;
    return r;
}

/*
 *--------------------------------------------------------------
 *
 * GetGenericBarePathBbox
 *
 *		Gets an overestimate of the bounding box rectangle of
 * 		a path assuming zero stroke width.
 * 		Untransformed coordinates!
 *
 * Results:
 *		A PathRect.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

PathRect
GetGenericBarePathBbox(PathAtom *atomPtr)
{
    double x1, y1, x2, y2, x3, y3, x4, y4, x5, y5;
    double currentX, currentY;
    PathRect r = {1.0e36, 1.0e36, -1.0e36, -1.0e36};
    
    currentX = 0.0;
    currentY = 0.0;

    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: { 
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                IncludePointInRect(&r, move->x, move->y);
                currentX = move->x;
                currentY = move->y;
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;

                IncludePointInRect(&r, line->x, line->y);
                currentX = line->x;
                currentY = line->y;
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                int result;
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
                    IncludePointInRect(&r, arc->x, arc->y);
                } else if (result == kPathArcOK) {
                    PathRect arcRect;
                    
                    arcRect = GetBareArcBbox(cx, cy, rx, ry, theta1, dtheta, 
                            DEGREES_TO_RADIANS * arc->angle);
                    IncludePointInRect(&r, arcRect.x1, arcRect.y1);
                    IncludePointInRect(&r, arcRect.x2, arcRect.y2);
                }
                currentX = arc->x;
                currentY = arc->y;
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                x1 = (currentX + quad->ctrlX)/2.0;
                y1 = (currentY + quad->ctrlY)/2.0;
                x2 = (quad->ctrlX + quad->anchorX)/2.0;
                y2 = (quad->ctrlY + quad->anchorY)/2.0;
                IncludePointInRect(&r, x1, y1);
                IncludePointInRect(&r, x2, y2);
                currentX = quad->anchorX;
                currentY = quad->anchorY;
                IncludePointInRect(&r, currentX, currentY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                x1 = (currentX + curve->ctrlX1)/2.0;
                y1 = (currentY + curve->ctrlY1)/2.0;
                x2 = (curve->ctrlX1 + curve->ctrlX2)/2.0;
                y2 = (curve->ctrlY1 + curve->ctrlY2)/2.0;
                x3 = (curve->ctrlX2 + curve->anchorX)/2.0;
                y3 = (curve->ctrlY2 + curve->anchorY)/2.0;
                IncludePointInRect(&r, x1, y1);
                IncludePointInRect(&r, x3, y3);
                x4 = (x1 + x2)/2.0;
                y4 = (y1 + y2)/2.0;
                x5 = (x2 + x3)/2.0;
                y5 = (y2 + y3)/2.0;
                IncludePointInRect(&r, x4, y4);
                IncludePointInRect(&r, x5, y5);
                currentX = curve->anchorX;
                currentY = curve->anchorY;
                IncludePointInRect(&r, currentX, currentY);
                break;
            }
            case PATH_ATOM_Z: {
                /* empty */
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    return r;
}

/*
 *--------------------------------------------------------------
 *
 * GetGenericPathTotalBboxFromBare --
 *
 *		This procedure calculates the items total bbox from the 
 *		bare bbox.
 *
 * Results:
 *		PathRect.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

PathRect
GetGenericPathTotalBboxFromBare(Tk_PathStyle *stylePtr, PathRect *bboxPtr)
{
    double fudge = 1.0;
    double width = 0.0;
    PathRect rect = *bboxPtr;
        
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
        if (width < 1.0) {
            width = 1.0;
        }
        rect.x1 -= width;
        rect.x2 += width;
        rect.y1 -= width;
        rect.y2 += width;
    }
    
    /* @@@ TODO: We should have a method here to add the necessary space
     * needed for sharp miter line joins.
     */
    
    /*
     * Add one (or two if antialiasing) more pixel of fudge factor just to be safe 
     * (e.g. X may round differently than we do).
     */
     
    if (gUseAntiAlias) {
        fudge = 2;
    }
    rect.x1 -= fudge;
    rect.x2 += fudge;
    rect.y1 -= fudge;
    rect.y2 += fudge;
    
    return rect;
}

/*
 *--------------------------------------------------------------
 *
 * SetGenericPathHeaderBbox --
 *
 *		This procedure sets the (transformed) bbox in the items header.
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

void
SetGenericPathHeaderBbox(
        Tk_Item *headerPtr,
        TMatrix *mPtr,
        PathRect *totalBboxPtr)
{
    PathRect rect;
    
    rect = *totalBboxPtr;

    if (mPtr != NULL) {
        double x, y;
        PathRect r = NewEmptyPathRect();

        /* Take each four corners in turn. */
        x = rect.x1, y = rect.y1;
        PathApplyTMatrix(mPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x2, y = rect.y1;
        PathApplyTMatrix(mPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x1, y = rect.y2;
        PathApplyTMatrix(mPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x2, y = rect.y2;
        PathApplyTMatrix(mPtr, &x, &y);
        IncludePointInRect(&r, x, y);
        rect = r;  
    }
    headerPtr->x1 = (int) rect.x1;
    headerPtr->x2 = (int) rect.x2;
    headerPtr->y1 = (int) rect.y1;
    headerPtr->y2 = (int) rect.y2;
}


/*
 *--------------------------------------------------------------
 *
 * GenericPathToPoint --
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

double
GenericPathToPoint(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against point. */
    Tk_PathStyle *stylePtr,
    PathAtom *atomPtr,
    int maxNumSegments,
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
    int				numPoints, numStrokes;
    int				isclosed;
    int				intersections, nonzerorule;
    int				sumIntersections = 0, sumNonzerorule = 0;
    double 			*polyPtr;
    double 			bestDist, radius, width, dist;
    Tk_State 		state = itemPtr->state;
    TMatrix 		*matrixPtr = stylePtr->matrixPtr;

    bestDist = 1.0e36;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        return bestDist;
    }
    if ((stylePtr->fillColor == NULL) && (stylePtr->strokeColor == NULL)) {
        return bestDist;
    }
    if (atomPtr == NULL) {
        return bestDist;
    }
    
    /* 
     * Do we need more memory or can we use static space? 
     */
    if (maxNumSegments > MAX_NUM_STATIC_SEGMENTS) {
        polyPtr = (double *) ckalloc((unsigned) (2*maxNumSegments*sizeof(double)));
    } else {
        polyPtr = staticSpace;
    }
    width = stylePtr->strokeWidth;
    if (width < 1.0) {
        width = 1.0;
    }
    radius = width/2.0;

    /*
     * Loop through each subpath, creating the approximate polyline,
     * and do the *ToPoint functions.
     *
     * Note: Strokes can be treated independently for each subpath,
     *		 but fills cannot since subpaths may intersect creating
     *		 "holes".
     */
     
#if PATH_DEBUG
    DebugPrintf(gInterp, 2, "PathToPoint..........");
#endif
    
    while (atomPtr != NULL) {
        MakeSubPathSegments(&atomPtr, polyPtr, &numPoints, &numStrokes, matrixPtr);
        isclosed = 0;
        if (numStrokes == numPoints) {
            isclosed = 1;
        }        
#if PATH_DEBUG
        {
            int i;
            
            DebugPrintf(gInterp, 2, "numPoints=%d, isclosed=%d, atomPtr=0x%.8x", numPoints, isclosed, atomPtr);
            for (i = 0; i < numPoints; i++) {
                DebugPrintf(gInterp, 2, "\t %6.1f, %6.1f", polyPtr[2*i], polyPtr[2*i+1]);
            }
        }
#endif        
        /*
         * This gives the min distance to the *stroke* AND the
         * number of intersections of the two types.
         */
        dist = PathPolygonToPointEx(polyPtr, numPoints, pointPtr, 
                &intersections, &nonzerorule);
        sumIntersections += intersections;
        sumNonzerorule += nonzerorule;
        if ((stylePtr->strokeColor != NULL) && (stylePtr->strokeWidth <= kPathStrokeThicknessLimit)) {
        
            /*
             * This gives the distance to a zero width polyline.
             * Use a simple scheme to adjust for a small width.
             */
            dist -= radius;
        }
        if (dist < bestDist) {
            bestDist = dist;
        }
        if (bestDist <= 0.0) {
            bestDist = 0.0;
            goto done;
        }

        /*
         * For wider strokes we must make a more detailed analysis.
         * Yes, there is an infinitesimal overlap to the above just
         * to be on the safe side.
         */
        if ((stylePtr->strokeColor != NULL) && (stylePtr->strokeWidth >= kPathStrokeThicknessLimit)) {
            dist = PathThickPolygonToPoint(stylePtr->joinStyle, stylePtr->capStyle, 
                    width, isclosed, polyPtr, numPoints, pointPtr);
            if (dist < bestDist) {
                bestDist = dist;
            }
            if (bestDist <= 0.0) {
                bestDist = 0.0;
                goto done;
            }
        }
    }        

    /*
     * We've processed all of the points.  
     * EvenOddRule: If the number of intersections is odd, 
     *			the point is inside the polygon.
     * WindingRule (nonzero): If the number of directed intersections
     *			are nonzero, then inside.
     */
    if (stylePtr->fillColor != NULL) {
        if ((stylePtr->fillRule == EvenOddRule) && (sumIntersections & 0x1)) {
            bestDist = 0.0;
        } else if ((stylePtr->fillRule == WindingRule) && (sumNonzerorule != 0)) {
            bestDist = 0.0;
        }
    }
    
done:
    if (polyPtr != staticSpace) {
        ckfree((char *) polyPtr);
    }
    return bestDist;
}

/*
 *--------------------------------------------------------------
 *
 * GenericPathToArea --
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

int
GenericPathToArea(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against line. */
    Tk_PathStyle *stylePtr,
    PathAtom *atomPtr,
    int maxNumSegments,
    double *areaPtr)		/* Pointer to array of four coordinates
                             * (x1, y1, x2, y2) describing rectangular
                             * area.  */
{
    int inside;				/* Tentative guess about what to return,
                             * based on all points seen so far:  one
                             * means everything seen so far was
                             * inside the area;  -1 means everything
                             * was outside the area.  0 means overlap
                             * has been found. */ 
    int				numPoints = 0;
    int				numStrokes = 0;
    int				isclosed = 0;
    double 			*polyPtr;
    double			currentT[2];
    Tk_State 		state = itemPtr->state;
    TMatrix 		*matrixPtr = stylePtr->matrixPtr;
    MoveToAtom		*move;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }    
    if (state == TK_STATE_HIDDEN) {
        return -1;
    }
    if ((stylePtr->fillColor == NULL) && (stylePtr->strokeColor == NULL)) {
        return -1;
    }
    if (atomPtr == NULL) {
        return -1;
    }
    
    /* 
     * Do we need more memory or can we use static space? 
     */
    if (maxNumSegments > MAX_NUM_STATIC_SEGMENTS) {
        polyPtr = (double *) ckalloc((unsigned) (2*maxNumSegments*sizeof(double)));
    } else {
        polyPtr = staticSpace;
    }

    /* A 'M' atom must be first, may show up later as well. */
    if (atomPtr->type != PATH_ATOM_M) {
        return -1;
    }
    move = (MoveToAtom *) atomPtr;
    PathApplyTMatrixToPoint(matrixPtr, &(move->x), currentT);
    
    /*
     * This defines the starting point. It is either -1 or 1. 
     * If any subseqent segment has a different 'inside'
     * then return 0 since one port (in|out)side and another
     * (out|in)side
     */
    inside = -1;
    if ((currentT[0] >= areaPtr[0]) && (currentT[0] <= areaPtr[2])
            && (currentT[1] >= areaPtr[1]) && (currentT[1] <= areaPtr[3])) {
        inside = 1;
    }
    
    while (atomPtr != NULL) {
        MakeSubPathSegments(&atomPtr, polyPtr, &numPoints, &numStrokes, matrixPtr);
        isclosed = 0;
        if (numStrokes == numPoints) {
            isclosed = 1;
        }        
        if (SubPathToArea(stylePtr, polyPtr, numPoints, numStrokes, 
                areaPtr, inside) != inside) {
            inside = 0;
            goto done;
        }
    }

done:
    if (polyPtr != staticSpace) {
        ckfree((char *) polyPtr);
    }
    return inside;
}

/*
 *--------------------------------------------------------------
 *
 * ArcSegments --
 *
 *		Given the arc parameters it makes a sequence if line segments.
 *		All angles in radians!
 *		Note that segments are transformed!
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the arc.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static void
ArcSegments(
    CentralArcPars *arcPars,
    TMatrix *matrixPtr,
    int includeFirst,			/* Should the first point be included? */
    int numSteps,				/* Number of curve segments to
                                 * generate.  */
    register double *coordPtr)	/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double cosPhi, sinPhi;
    double cosAlpha, sinAlpha;
    double alpha, dalpha, theta1;
    double cx, cy, rx, ry;
    
    cosPhi = cos(arcPars->phi);
    sinPhi = sin(arcPars->phi);
    cx = arcPars->cx;
    cy = arcPars->cy;
    rx = arcPars->rx;
    ry = arcPars->ry;
    theta1 = arcPars->theta1;
    dalpha = arcPars->dtheta/numSteps;

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        alpha = theta1 + i*dalpha;
        cosAlpha = cos(alpha);
        sinAlpha = sin(alpha);
        coordPtr[0] = cx + rx*cosAlpha*cosPhi - ry*sinAlpha*sinPhi;
        coordPtr[1] = cy + rx*cosAlpha*sinPhi + ry*sinAlpha*cosPhi;
        PathApplyTMatrix(matrixPtr, coordPtr, coordPtr+1);
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

/*
 *--------------------------------------------------------------
 *
 * CurveSegments --
 *
 *		Given four control points, create a larger set of points
 *		for a cubic Bezier spline based on the points.
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the Bezier spline defined
 *		by the four control points.  
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

void
CurveSegments(
    double control[],		/* Array of coordinates for four
                             * control points:  x0, y0, x1, y1,
                             * ... x3 y3. */
    int includeFirst,		/* Should the first point be included? */
    int numSteps,			/* Number of curve segments to
                             * generate.  */
    register double *coordPtr)		/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double u, u2, u3, t, t2, t3;
    
    /*
     * We should use the 'de Castlejau' algorithm to iterate
     * line segments until a certain tolerance.
     */

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        t = ((double) i)/((double) numSteps);
        t2 = t*t;
        t3 = t2*t;
        u = 1.0 - t;
        u2 = u*u;
        u3 = u2*u;
        coordPtr[0] = control[0]*u3
                + 3.0 * (control[2]*t*u2 + control[4]*t2*u) + control[6]*t3;
        coordPtr[1] = control[1]*u3
                + 3.0 * (control[3]*t*u2 + control[5]*t2*u) + control[7]*t3;
    }
}

/*
 *--------------------------------------------------------------
 *
 * QuadBezierSegments --
 *
 *		Given three control points, create a larger set of points
 *		for a quadratic Bezier spline based on the points.
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the quadratic Bezier spline defined
 *		by the control points.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static void
QuadBezierSegments(
    double control[],			/* Array of coordinates for three
                                 * control points:  x0, y0, x1, y1,
                                 * x2, y2. */
    int includeFirst,			/* Should the first point be included? */
    int numSteps,				/* Number of curve segments to
                                 * generate.  */
    register double *coordPtr)	/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double u, u2, t, t2;

#if PATH_DEBUG
    DebugPrintf(gInterp, 2, "QuadBezierSegments %6.0f, %6.0f, %6.0f, %6.0f, %6.0f, %6.0f", 
            control[0], control[1], control[2], control[3], control[4], control[5]);
#endif

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        t = ((double) i)/((double) numSteps);
        t2 = t*t;
        u = 1.0 - t;
        u2 = u*u;
        coordPtr[0] = control[0]*u2 + 2.0 * control[2]*t*u + control[4]*t2;
        coordPtr[1] = control[1]*u2 + 2.0 * control[3]*t*u + control[5]*t2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * MakeSubPathSegments --
 *
 *		Supposed to be a generic segment generator that can be used 
 *		by both Area and Point functions.
 *
 * Results:
 *		Points filled into polyPtr...
 *
 * Side effects:
 *		Pointer *atomPtrPtr may be updated.
 *
 *--------------------------------------------------------------
 */

static void
MakeSubPathSegments(PathAtom **atomPtrPtr, double *polyPtr, 
        int *numPointsPtr, int *numStrokesPtr, TMatrix *matrixPtr)
{
    int 			first = 1;
    int				numPoints;
    int				numStrokes;
    int				numAdded;
    int				isclosed = 0;
    double 			current[2];		/* Current untransformed point. */
    double			*currentTPtr;	/* Pointer to the transformed current point. */
    double			*coordPtr;
    PathAtom 		*atomPtr;
    MoveToAtom 		*move;
    LineToAtom 		*line;
    ArcAtom 		*arc;
    QuadBezierAtom 	*quad;
    CurveToAtom 	*curve;
    CloseAtom		*close;
    
    /* @@@ 	Note that for unfilled paths we could have made a progressive
     *     	area (point) check which may be faster since we may stop when 0 (overlapping).
     *	   	For filled paths we cannot rely on this since the area rectangle
     *		may be entirely enclosed in the path and still overlapping.
     *		(Need better explanation!)
     */
    
    /*
     * Check each segment of the path.
     * Any transform matrix is applied at the last stage when comparing to rect.
     * 'current' is always untransformed coords.
     */

    current[0] = 0.0;
    current[1] = 0.0;
    numPoints = 0;
    numStrokes = 0;
    isclosed = 0;
    atomPtr = *atomPtrPtr;
    coordPtr = NULL;
    
    while (atomPtr != NULL) {

#if PATH_DEBUG    
        DebugPrintf(gInterp, 2, "atomPtr->type %c", atomPtr->type);
#endif

        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
            
                /* A 'M' atom must be first, may show up later as well. */
                
                if (first) {
                    coordPtr = polyPtr;
                    current[0] = move->x;
                    current[1] = move->y;
                    PathApplyTMatrixToPoint(matrixPtr, current, coordPtr);
                    currentTPtr = coordPtr;
                    coordPtr += 2;
                    numPoints = 1;
                } else {
                
                    /*  
                     * We have finalized a subpath.
                     */
                    goto done;
                }
                first = 0;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                PathApplyTMatrixToPoint(matrixPtr, &(line->x), coordPtr);
                current[0] = line->x;
                current[1] = line->y;
                currentTPtr = coordPtr;
                coordPtr += 2;
                numPoints++;;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                numAdded = AddArcSegments(matrixPtr, current, arc, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = arc->x;
                current[1] = arc->y;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                numAdded = AddQuadBezierSegments(matrixPtr, current,
                        quad, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = quad->anchorX;
                current[1] = quad->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                numAdded = AddCurveToSegments(matrixPtr, current,
                        curve, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = curve->anchorX;
                current[1] = curve->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Z: {
            
                /* Just add the first point to the end. */
                close = (CloseAtom *) atomPtr;
                coordPtr[0] = polyPtr[0];
                coordPtr[1] = polyPtr[1];
                coordPtr += 2;
                numPoints++;
                current[0]  = close->x;
                current[1]  = close->y;
                isclosed = 1;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }

done:
    if (numPoints > 1) {
        if (isclosed) {
            numStrokes = numPoints;
        } else {
            numStrokes = numPoints - 1;
        }
    }
    *numPointsPtr = numPoints;
    *numStrokesPtr = numStrokes;
    *atomPtrPtr = atomPtr;

    return;
}

/*
 *--------------------------------------------------------------
 *
 * AddArcSegments, AddQuadBezierSegments, AddCurveToSegments --
 *
 *		Adds a number of points along the arc (curve) to coordPtr
 *		representing straight line segments.
 *
 * Results:
 *		Number of points added. 
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
AddArcSegments(
    TMatrix *matrixPtr,
    double current[2],		/* Current point. */
    ArcAtom *arc,
    double *coordPtr)		/* Where to put the points. */
{
    int result;
    int numPoints;
    CentralArcPars arcPars;
    double cx, cy, rx, ry;
    double theta1, dtheta;
            
    /*
     * Note: The arc parametrization used cannot generally
     * be transformed. Need to transform each line segment separately!
     */
    
    result = EndpointToCentralArcParameters(
            current[0], current[1],
            arc->x, arc->y, arc->radX, arc->radY, 
            DEGREES_TO_RADIANS * arc->angle, 
            arc->largeArcFlag, arc->sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
    if (result == kPathArcLine) {
        double pts[2];

        pts[0] = arc->x;
        pts[1] = arc->y;
        PathApplyTMatrix(matrixPtr, pts, pts+1);
        coordPtr[0] = pts[0];
        coordPtr[1] = pts[1];
        return 1;
    } else if (result == kPathArcSkip) {
        return 0;
    }

    arcPars.cx = cx;
    arcPars.cy = cy;
    arcPars.rx = rx;
    arcPars.ry = ry;
    arcPars.theta1 = theta1;
    arcPars.dtheta = dtheta;
    arcPars.phi = arc->angle;

    numPoints = GetArcNumSegments(current[0], current[1], arc);    
    ArcSegments(&arcPars, matrixPtr, 0, numPoints, coordPtr);

    return numPoints;
}

static int
AddQuadBezierSegments(
    TMatrix *matrixPtr,
    double current[2],		/* Current point. */
    QuadBezierAtom *quad,
    double *coordPtr)		/* Where to put the points. */
{
    int numPoints;			/* Number of curve points to
                             * generate.  */
    double control[6];

    PathApplyTMatrixToPoint(matrixPtr, current, control);
    PathApplyTMatrixToPoint(matrixPtr, &(quad->ctrlX), control+2);
    PathApplyTMatrixToPoint(matrixPtr, &(quad->anchorX), control+4);

    numPoints = kPathNumSegmentsQuadBezier;
    QuadBezierSegments(control, 0, numPoints, coordPtr);
    
#if PATH_DEBUG
        {
            int i;
            
            DebugPrintf(gInterp, 2, "AddQuadBezierSegments: numPoints=%d", numPoints);
            for (i = 0; i < numPoints; i++) {
                DebugPrintf(gInterp, 2, "\t %6.1f, %6.1f", coordPtr[2*i], coordPtr[2*i+1]);
            }
        }
#endif

    return numPoints;
}

static int
AddCurveToSegments(
    TMatrix *matrixPtr,
    double current[2],			/* Current point. */
    CurveToAtom *curve,
    double *coordPtr)
{
    int numSteps;				/* Number of curve points to
                                 * generate.  */
    double control[8];

    PathApplyTMatrixToPoint(matrixPtr, current, control);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->ctrlX1), control+2);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->ctrlX2), control+4);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->anchorX), control+6);

    numSteps = kPathNumSegmentsCurveTo;
    CurveSegments(control, 1, numSteps, coordPtr);
    
    return numSteps;
}

/*
 *--------------------------------------------------------------
 *
 * SubPathToArea --
 *
 *		This procedure is called to determine whether a subpath
 *		lies entirely inside, entirely outside, or overlapping
 *		a given rectangular area.
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
SubPathToArea(
    Tk_PathStyle *stylePtr,
    double 		*polyPtr, 
    int 		numPoints, 		/* Total number of points. First one
                                 * is duplicated in the last. */
    int			numStrokes,		/* The number of strokes which is one less
                                 * than numPoints if path not closed. */
    double 		*rectPtr, 
    int 		inside)			/* This is the current inside status. */
{
    double width;
    
    /* @@@ 	There is an open question how a closed unfilled polygon
     *		completely enclosing the area rect should be counted.
     *		While the tk canvas polygon item counts it as intersecting (0),
     *		the line item counts it as outside (-1).
     */
    
    if (stylePtr->fillColor != NULL) {
    
        /* This checks a closed polygon with zero width for inside.
         * If area rect completely enclosed it returns intersecting (0).
         */
        if (TkPolygonToArea(polyPtr, numPoints, rectPtr) != inside) {
            return 0;
        }
    }
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
        if (width < 1.0) {
            width = 1.0;
        }
        if (stylePtr->strokeWidth > kPathStrokeThicknessLimit) {
            if (TkThickPolyLineToArea(polyPtr, numStrokes, 
                    width, stylePtr->capStyle, 
                    stylePtr->joinStyle, rectPtr) != inside) {
                return 0;
            }
        } else {
			if (PathPolyLineToArea(polyPtr, numStrokes, rectPtr) != inside) {
                return 0;
            }
        }
    }
    return inside;
}

/*---------------------------*/

/*
 *--------------------------------------------------------------
 *
 * TranslatePathAtoms --
 *
 *		This procedure is called to translate a linked list of path atoms.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Path atoms changed.
 *
 *--------------------------------------------------------------
 */

void
TranslatePathAtoms(
    PathAtom *atomPtr,
    double deltaX,				/* Amount by which item is to be */
    double deltaY)              /* moved. */
{
    while (atomPtr != NULL) {
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                move->x += deltaX;
                move->y += deltaY;
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                line->x += deltaX;
                line->y += deltaY;
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                
                arc->x += deltaX;
                arc->y += deltaY;
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                quad->ctrlX += deltaX;
                quad->ctrlY += deltaY;
                quad->anchorX += deltaX;
                quad->anchorY += deltaY;
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                curve->ctrlX1 += deltaX;
                curve->ctrlY1 += deltaY;
                curve->ctrlX2 += deltaX;
                curve->ctrlY2 += deltaY;
                curve->anchorX += deltaX;
                curve->anchorY += deltaY;
                break;
            }
            case PATH_ATOM_Z: {
                CloseAtom *close = (CloseAtom *) atomPtr;
                
                close->x += deltaX;
                close->y += deltaY;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScalePathAtoms --
 *
 *		This procedure is called to scale a linked list of path atoms.
 *		The following transformation is applied to all point
 *		coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Path atoms changed.
 *
 *--------------------------------------------------------------
 */

void
ScalePathAtoms(
    PathAtom *atomPtr,
    double originX, double originY,	/* Origin about which to scale rect. */
    double scaleX,			/* Amount to scale in X direction. */
    double scaleY)			/* Amount to scale in Y direction. */
{
    while (atomPtr != NULL) {
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                move->x = originX + scaleX*(move->x - originX);
                move->y = originY + scaleY*(move->y - originY);
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                line->x = originX + scaleX*(line->x - originX);
                line->y = originY + scaleY*(line->y - originY);
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                /* INCOMPLETE !!!!!!!!!!!*/
                /* WRONG !!!!!!!!!!!!!!!*/
                arc->radX = scaleX*arc->radX;
                arc->radY = scaleY*arc->radY;
                arc->x = originX + scaleX*(arc->x - originX);
                arc->y = originY + scaleY*(arc->y - originY);
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                quad->ctrlX = originX + scaleX*(quad->ctrlX - originX);
                quad->ctrlY = originY + scaleY*(quad->ctrlY - originY);
                quad->anchorX = originX + scaleX*(quad->anchorX - originX);
                quad->anchorY = originY + scaleY*(quad->anchorY - originY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                curve->ctrlX1 = originX + scaleX*(curve->ctrlX1 - originX);
                curve->ctrlY1 = originY + scaleY*(curve->ctrlY1 - originY);
                curve->ctrlX2 = originX + scaleX*(curve->ctrlX2 - originX);
                curve->ctrlY2 = originY + scaleY*(curve->ctrlY2 - originY);
                curve->anchorX = originX + scaleX*(curve->anchorX - originX);
                curve->anchorY = originY + scaleY*(curve->anchorY - originY);
                break;
            }
            case PATH_ATOM_Z: {
                CloseAtom *close = (CloseAtom *) atomPtr;
                
                close->x = originX + scaleX*(close->x - originX);
                close->y = originY + scaleY*(close->y - originY);
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
}

/*------------------*/

TMatrix
GetCanvasTMatrix(Tk_Canvas canvas)
{
    short originX, originY;
    TMatrix m;
    
    /* @@@ Any scaling involved as well??? */
    Tk_CanvasDrawableCoords(canvas, 0.0, 0.0, &originX, &originY);

    m = kPathUnitTMatrix;
    m.tx = originX;
    m.ty = originY;
    
    return m;
}

PathRect
NewEmptyPathRect(void)
{
    PathRect r;
    
    r.x1 = 1.0e36;
    r.y1 = 1.0e36;
    r.x2 = -1.0e36;
    r.y2 = -1.0e36;
    return r;
}

void
IncludePointInRect(PathRect *r, double x, double y)
{
    r->x1 = MIN(r->x1, x);
    r->y1 = MIN(r->y1, y);
    r->x2 = MAX(r->x2, x);
    r->y2 = MAX(r->y2, y);
}

void
TranslatePathRect(PathRect *r, double deltaX, double deltaY)
{
    r->x1 += deltaX;
    r->x2 += deltaX;
    r->y1 += deltaY;
    r->y2 += deltaY;
}


/*
 * +++ A bunch of custum option processing functions needed +++
 */
 
/*
 *--------------------------------------------------------------
 *
 * FillRuleParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-fillrule" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

int
FillRuleParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    int c;
    size_t length;
    
    register int *fillRulePtr = (int *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        *fillRulePtr = WindingRule;
        return TCL_OK;
    }

    c = value[0];
    length = strlen(value);

    if ((c == 'n') && (strncmp(value, "nonzero", length) == 0)) {
        *fillRulePtr = WindingRule;
        return TCL_OK;
    }
    if ((c == 'e') && (strncmp(value, "evenodd", length) == 0)) {
        *fillRulePtr = EvenOddRule;
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad value \"", value, 
            "\": must be \"nonzero\" or \"evenodd\"",
	    (char *) NULL);
        *fillRulePtr = WindingRule;
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * FillRulePrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-fillrule"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

char *
FillRulePrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register int *fillRulePtr = (int *) (widgRec + offset);
    *freeProcPtr = NULL;

    if (*fillRulePtr == WindingRule) {
        return "nonzero";
    } else if (*fillRulePtr == EvenOddRule) {
        return "evenodd";
    } else {
        return "";
    }
}

/*
 *--------------------------------------------------------------
 *
 * LinearGradientParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-lineargradient" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

int
LinearGradientParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;    
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
        if (HaveLinearGradientStyleWithName(value) != TCL_OK) {
            Tcl_AppendResult(interp, "bad value \"", value, 
                    "\": does not exist",
                    (char *) NULL);
            return TCL_ERROR;
        } else {
            new = (char *) ckalloc((unsigned) (strlen(value) + 1));
            strcpy(new, value);
        }
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LinearGradientPrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-lineargradient"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

char *
LinearGradientPrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    char *result;
    register char *ptr = (char *) (widgRec + offset);

    result = (*(char **) ptr);
    if (result == NULL) {
        result = "";
    }
    return result;
}


/*
 *--------------------------------------------------------------
 *
 * StyleParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-style" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

int
StyleParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;    
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
    
        /* 
         * We only check that the style name exist here and
         * do the processing after configuration.
         */
        if (PathStyleHaveWithName(value) != TCL_OK) {
            Tcl_AppendResult(interp, "bad value \"", value, 
                    "\": does not exist",
                    (char *) NULL);
            return TCL_ERROR;
        } else {
            new = (char *) ckalloc((unsigned) (strlen(value) + 1));
            strcpy(new, value);
        }
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * StylePrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-style"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

char *
StylePrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    char *result;
    register char *ptr = (char *) (widgRec + offset);

    result = (*(char **) ptr);
    if (result == NULL) {
        result = "";
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * MatrixParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-matrix" option. It translates the (string) option 
 *		into a double array.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

int
MatrixParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;
    TMatrix *matrixPtr;
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
        matrixPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
        if (PathGetTMatrix(interp, value, matrixPtr) != TCL_OK) {
            ckfree((char *) matrixPtr);
            return TCL_ERROR;
        }
        new = (char *) matrixPtr;
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    
    return TCL_OK;
}

char *
MatrixPrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,				/* Pointer to record for item. */
    int offset,					/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)	/* Pointer to variable to fill in with
                                 * information about how to reclaim
                                 * storage for return string. */
{
    char *buffer, *str;
    int len;
    TMatrix *matrixPtr;
    Tcl_Obj *listObj;
    register char *ptr = (char *) (widgRec + offset);

    *freeProcPtr = TCL_DYNAMIC;

    matrixPtr = (*(TMatrix **) ptr); 
    PathGetTclObjFromTMatrix(NULL, matrixPtr, &listObj);
    str = Tcl_GetStringFromObj(listObj, &len);
    buffer = (char *) ckalloc((unsigned int) (len + 1));
    strcpy(buffer, str);
    Tcl_DecrRefCount(listObj);

    return buffer;
}


/*
 *--------------------------------------------------------------
 *
 * PathPolyLineToArea --
 *
 *		Determine whether an open polygon lies entirely inside, entirely
 *		outside, or overlapping a given rectangular area.
 * 		Identical to TkPolygonToArea except that it returns outside (-1)
 *		if completely encompassing the area rect.
 *
 * Results:
 *		-1 is returned if the polygon given by polyPtr and numPoints
 *		is entirely outside the rectangle given by rectPtr.  0 is
 *		returned if the polygon overlaps the rectangle, and 1 is
 *		returned if the polygon is entirely inside the rectangle.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

int
PathPolyLineToArea(
    double *polyPtr,		/* Points to an array coordinates for
                             * closed polygon:  x0, y0, x1, y1, ...
                             * The polygon may be self-intersecting. */
    int numPoints,			/* Total number of points at *polyPtr. */
    register double *rectPtr)	/* Points to coords for rectangle, in the
                             * order x1, y1, x2, y2.  X1 and y1 must
                             * be lower-left corner. */
{
    int state;				/* State of all edges seen so far (-1 means
                             * outside, 1 means inside, won't ever be
                             * 0). */
    int count;
    register double *pPtr;

    /*
     * Iterate over all of the edges of the polygon and test them
     * against the rectangle.  Can quit as soon as the state becomes
     * "intersecting".
     */

    state = TkLineToArea(polyPtr, polyPtr+2, rectPtr);
    if (state == 0) {
        return 0;
    }
    for (pPtr = polyPtr+2, count = numPoints-1; count >= 2;
            pPtr += 2, count--) {
        if (TkLineToArea(pPtr, pPtr+2, rectPtr) != state) {
            return 0;
        }
    }
    return state;
}

/*
 *--------------------------------------------------------------
 *
 * PathThickPolygonToPoint --
 *
 *		Computes the distance from a given point to a given
 *		thick polyline (open or closed), in canvas units.
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

double
PathThickPolygonToPoint(
    int joinStyle, int capStyle, 
    double width,
    int isclosed,
    double *polyPtr,	/* Points to an array coordinates for
                         * the polygon:  x0, y0, x1, y1, ...
                         * The polygon may be self-intersecting. */
    int numPoints,		/* Total number of points at *polyPtr. */
    double *pointPtr)	/* Points to coords for point. */
{
    int count;
    int project;
    int testrounding;
    int changedMiterToBevel;	/* Non-zero means that a mitered corner
                                 * had to be treated as beveled after all
                                 * because the angle was < 11 degrees. */
    double bestDist;			/* Closest distance between point and
                                 * any edge in polygon. */
    double dist, radius;
    double *coordPtr;
    double poly[10];
    
    bestDist = 1.0e36;
    radius = width/2.0;
    project = 0;
    if (!isclosed) {
        project = (capStyle == CapProjecting);
    }

    /*
     * The overall idea is to iterate through all of the edges of
     * the line, computing a polygon for each edge and testing the
     * point against that polygon.  In addition, there are additional
     * tests to deal with rounded joints and caps.
     */

    changedMiterToBevel = 0;
    for (count = numPoints, coordPtr = polyPtr; count >= 2;
            count--, coordPtr += 2) {
    
        /*
         * If rounding is done around the first point then compute
         * the distance between the point and the point.
         */
        testrounding = 0;
        if (isclosed) {
            testrounding = (joinStyle == JoinRound);
        } else {
            testrounding = (((capStyle == CapRound) && (count == numPoints))
                    || ((joinStyle == JoinRound) && (count != numPoints)));
        }    
        if (testrounding) {
            dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
                    - radius;
            if (dist <= 0.0) {
                bestDist = 0.0;
                goto donepoint;
            } else if (dist < bestDist) {
                bestDist = dist;
            }
        }
    
        /*
        * Compute the polygonal shape corresponding to this edge,
        * consisting of two points for the first point of the edge
        * and two points for the last point of the edge.
        */
    
        if (count == numPoints) {
            TkGetButtPoints(coordPtr+2, coordPtr, (double) width,
                    project, poly, poly+2);
        } else if ((joinStyle == JoinMiter) && !changedMiterToBevel) {
            poly[0] = poly[6];
            poly[1] = poly[7];
            poly[2] = poly[4];
            poly[3] = poly[5];
        } else {
            TkGetButtPoints(coordPtr+2, coordPtr, (double) width, 0,
                    poly, poly+2);
    
            /*
             * If this line uses beveled joints, then check the distance
             * to a polygon comprising the last two points of the previous
             * polygon and the first two from this polygon;  this checks
             * the wedges that fill the mitered joint.
             */
    
            if ((joinStyle == JoinBevel) || changedMiterToBevel) {
                poly[8] = poly[0];
                poly[9] = poly[1];
                dist = TkPolygonToPoint(poly, 5, pointPtr);
                if (dist <= 0.0) {
                    bestDist = 0.0;
                    goto donepoint;
                } else if (dist < bestDist) {
                    bestDist = dist;
                }
                changedMiterToBevel = 0;
            }
        }
        if (count == 2) {
            TkGetButtPoints(coordPtr, coordPtr+2, (double) width,
                    project, poly+4, poly+6);
        } else if (joinStyle == JoinMiter) {
            if (TkGetMiterPoints(coordPtr, coordPtr+2, coordPtr+4,
                    (double) width, poly+4, poly+6) == 0) {
                changedMiterToBevel = 1;
                TkGetButtPoints(coordPtr, coordPtr+2, (double) width,
                        0, poly+4, poly+6);
            }
        } else {
            TkGetButtPoints(coordPtr, coordPtr+2, (double) width, 0,
                    poly+4, poly+6);
        }
        poly[8] = poly[0];
        poly[9] = poly[1];
        dist = TkPolygonToPoint(poly, 5, pointPtr);
        if (dist <= 0.0) {
            bestDist = 0.0;
            goto donepoint;
        } else if (dist < bestDist) {
            bestDist = dist;
        }
    }
        
    /*
     * If caps are rounded, check the distance to the cap around the
     * final end point of the line.
     */
    if (!isclosed && (capStyle == CapRound)) {
        dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
                - width/2.0;
        if (dist <= 0.0) {
            bestDist = 0.0;
            goto donepoint;
        } else if (dist < bestDist) {
            bestDist = dist;
        }
    }

donepoint:

    return bestDist;
}

/*
 *--------------------------------------------------------------
 *
 * PathPolygonToPointEx --
 *
 *		Compute the distance from a point to a polygon. This is
 *		essentially identical to TkPolygonToPoint with two exceptions:
 *		1) 	It returns the closest distance to the *stroke*,
 *			any fill unrecognized.
 *		2)	It returns both number of total intersections, and
 *			the number of directed crossings, nonzerorule.
 *
 * Results:
 *		The return value is 0.0 if the point referred to by
 *		pointPtr is within the polygon referred to by polyPtr
 *		and numPoints.  Otherwise the return value is the
 *		distance of the point from the polygon.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

double
PathPolygonToPointEx(
    double *polyPtr,	/* Points to an array coordinates for
                         * the polygon:  x0, y0, x1, y1, ...
                         * The polygon may be self-intersecting.
                         * If a fillRule is used the last point
                         * must duplicate the first one. */
    int numPoints,		/* Total number of points at *polyPtr. */
    double *pointPtr,	/* Points to coords for point. */
    int *intersectionsPtr,	/* (out) The number of intersections. */
    int *nonzerorulePtr)	/* (out) The number of intersections
                             * considering crossing direction. */
{
    double bestDist;		/* Closest distance between point and
                             * any edge in polygon. */
    int intersections;		/* Number of edges in the polygon that
                             * intersect a ray extending vertically
                             * upwards from the point to infinity. */
    int nonzerorule;		/* As 'intersections' except that it adds
                             * one if crossing right to left, and
                             * subtracts one if crossing left to right. */
    int count;
    register double *pPtr;

    /*
     * Iterate through all of the edges in the polygon, updating
     * bestDist and intersections.
     *
     * TRICKY POINT:  when computing intersections, include left
     * x-coordinate of line within its range, but not y-coordinate.
     * Otherwise if the point lies exactly below a vertex we'll
     * count it as two intersections.
     */

    bestDist = 1.0e36;
    intersections = 0;
    nonzerorule = 0;

    for (count = numPoints, pPtr = polyPtr; count > 1; count--, pPtr += 2) {
        double x, y, dist;
    
        /*
         * Compute the point on the current edge closest to the point
         * and update the intersection count.  This must be done
         * separately for vertical edges, horizontal edges, and
         * other edges.
         */
    
        if (pPtr[2] == pPtr[0]) {
    
            /*
             * Vertical edge.
             */
    
            x = pPtr[0];
            if (pPtr[1] >= pPtr[3]) {
                y = MIN(pPtr[1], pointPtr[1]);
                y = MAX(y, pPtr[3]);
            } else {
                y = MIN(pPtr[3], pointPtr[1]);
                y = MAX(y, pPtr[1]);
            }
        } else if (pPtr[3] == pPtr[1]) {
    
            /*
             * Horizontal edge.
             */
    
            y = pPtr[1];
            if (pPtr[0] >= pPtr[2]) {
                x = MIN(pPtr[0], pointPtr[0]);
                x = MAX(x, pPtr[2]);
                if ((pointPtr[1] < y) && (pointPtr[0] < pPtr[0])
                        && (pointPtr[0] >= pPtr[2])) {
                    intersections++;
                    nonzerorule++;
                }
            } else {
                x = MIN(pPtr[2], pointPtr[0]);
                x = MAX(x, pPtr[0]);
                if ((pointPtr[1] < y) && (pointPtr[0] < pPtr[2])
                        && (pointPtr[0] >= pPtr[0])) {
                    intersections++;
                    nonzerorule--;
                }
            }
        } else {
            double m1, b1, m2, b2;
            int lower;			/* Non-zero means point below line. */
    
            /*
             * The edge is neither horizontal nor vertical.  Convert the
             * edge to a line equation of the form y = m1*x + b1.  Then
             * compute a line perpendicular to this edge but passing
             * through the point, also in the form y = m2*x + b2.
             */
    
            m1 = (pPtr[3] - pPtr[1])/(pPtr[2] - pPtr[0]);
            b1 = pPtr[1] - m1*pPtr[0];
            m2 = -1.0/m1;
            b2 = pointPtr[1] - m2*pointPtr[0];
            x = (b2 - b1)/(m1 - m2);
            y = m1*x + b1;
            if (pPtr[0] > pPtr[2]) {
                if (x > pPtr[0]) {
                    x = pPtr[0];
                    y = pPtr[1];
                } else if (x < pPtr[2]) {
                    x = pPtr[2];
                    y = pPtr[3];
                }
            } else {
                if (x > pPtr[2]) {
                    x = pPtr[2];
                    y = pPtr[3];
                } else if (x < pPtr[0]) {
                    x = pPtr[0];
                    y = pPtr[1];
                }
            }
            lower = (m1*pointPtr[0] + b1) > pointPtr[1];
            if (lower && (pointPtr[0] >= MIN(pPtr[0], pPtr[2]))
                    && (pointPtr[0] < MAX(pPtr[0], pPtr[2]))) {
                intersections++;
                if (pPtr[0] >= pPtr[2]) {
                    nonzerorule++;
                } else {
                    nonzerorule--;
                }
            }
        }
    
        /*
         * Compute the distance to the closest point, and see if that
         * is the best distance seen so far.
         */
    
        dist = hypot(pointPtr[0] - x, pointPtr[1] - y);
        if (dist < bestDist) {
            bestDist = dist;
        }
    }
    *intersectionsPtr = intersections;
    *nonzerorulePtr = nonzerorule;
    
    return bestDist;
}

/*--------------------------------------------------------------------------*/
