/*
 * tkPathTkDraw.c --
 *
 *		This file implements a path canvas item modelled after its
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <tcl.h>
#include <tk.h>
#include <tkInt.h>
#include "tkPort.h"
#include "tkCanvas.h"
#include "tkPath.h"
#include "tkIntPath.h"


extern int gUseAntiAlias;


void TkPathInit(Display *display, Drawable d)
{

}

void
TkPathPushTMatrix(Drawable d, TMatrix *m)
{

}

void TkPathBeginPath(Drawable d, Tk_PathStyle *style)
{

}

void TkPathMoveTo(Drawable d, double x, double y)
{

}

void TkPathLineTo(Drawable d, double x, double y)
{

}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double cx, cy;
    double x31, y31, x32, y32;
    
    //cairo_current_point(gctx, &cx, &cy);

    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = cx + (ctrlX - cx) * 2 / 3;
    y31 = cy + (ctrlY - cy) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;


}

void TkPathCurveTo(Drawable d, double x1, double y1, 
        double x2, double y2, double x, double y)
{

}

void TkPathArcTo(Drawable d,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x, double y)
{
    TkPathArcToUsingBezier(d, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void TkPathClosePath(Drawable d)
{

}

void TkPathClipToPath(Drawable d, int fillRule)
{

}

void TkPathReleaseClipToPath(Drawable d)
{

}

void TkPathStroke(Drawable d, Tk_PathStyle *style)
{       

}

void TkPathFill(Drawable d, Tk_PathStyle *style)
{

}

void TkPathFillAndStroke(Drawable d, Tk_PathStyle *style)
{

}

void TkPathEndPath(Drawable d)
{
    /* Empty ??? */
}

void TkPathFree(Drawable d)
{

}

int TkPathDrawingDestroysPath(void)
{
    return 0;
}

int TkPathGetCurrentPosition(Drawable d, PathPoint *pt)
{

    return TCL_OK;
}

int TkPathBoundingBox(PathRect *rPtr)
{
    return TCL_ERROR;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{    

}
            

