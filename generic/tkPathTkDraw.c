/*
 * tkPathTkDraw.c --
 *
 *		This file implements a path canvas item modelled after its
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *		
 * 	Note:
 *		This is supposed to be a minimal implementation using
 *		Tk drawing only. It fails in a number of places such as
 *		filled and overlapping subpaths.
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

#define _PATH_N_BUFFER_POINTS 	2000

extern int gUseAntiAlias;

extern void	CurveSegments(double control[], int includeFirst, int numSteps, register double *coordPtr);

/*
 * Each subpath is reconstructed as a number of straight line segments.
 * These are always stored as transformed coordinates.
 */
typedef struct _PathSegments {
    int 			npoints;
    double 			*points;
    int				isclosed;
    struct _PathSegments *next;
} _PathSegments;

/*
 * A placeholder for the context we are working in.
 * The current and lastMove are always original untransformed coordinates.
 */
typedef struct _PathContext {
    Display 		*display;
    Drawable 		drawable;
    double 			current[2];
    double 			lastMove[2];
    int				hasCurrent;
    TMatrix 		*m;
    _PathSegments 	*segm;
    _PathSegments 	*currentSegm;
} _PathContext;

static _PathContext *gctx = NULL;

static _PathContext* _NewPathContext(Display *display, Drawable drawable)
{
    _PathContext *ctx;
    
    ctx = (_PathContext *) ckalloc(sizeof(_PathContext));
    ctx->display = display;
    ctx->drawable = drawable;
    ctx->current[0] = 0.0;
    ctx->current[1] = 0.0;
    ctx->lastMove[0] = 0.0;
    ctx->lastMove[1] = 0.0;
    ctx->hasCurrent = 0;
    ctx->m = NULL;
    ctx->segm = NULL;
    ctx->currentSegm = NULL;
    return ctx;
}

static _PathSegments* _NewPathSegments(void)
{
    _PathSegments *segm;
    
    segm = (_PathSegments *) ckalloc(sizeof(_PathSegments));
    segm->npoints = 0;
    segm->points = (double *) ckalloc((unsigned) (2*_PATH_N_BUFFER_POINTS*sizeof(double)));
    segm->isclosed = 0;
    segm->next = NULL;
    return segm;
}

static void _PathContextFree(_PathContext *ctx)
{
    _PathSegments *tmpSegm, *segm;

    segm = ctx->segm;
    while (segm != NULL) {
        tmpSegm = segm;
        segm = tmpSegm->next;
        ckfree((char *) tmpSegm->points);
        ckfree((char *) tmpSegm);
    }
    if (ctx->m != NULL) {
        ckfree((char *) ctx->m);
    }
    ckfree((char *) ctx);
}

static double* _CheckCoordSpace(_PathSegments *segm, int numPoints)
{

    //char *ckrealloc(ptr, size)

    //_PATH_N_BUFFER_POINTS
}

void TkPathInit(Display *display, Drawable d)
{
    gctx = _NewPathContext(display, d);
}

void
TkPathPushTMatrix(Drawable d, TMatrix *m)
{
    if (gctx->m == NULL) {
        gctx->m = (TMatrix *) ckalloc(sizeof(TMatrix));
        *(gctx->m) = *m;
    } else {
        TMatrix tmp = *(gctx->m);
        TMatrix *p = gctx->m;
        
        p->a  = m->a*tmp.a  + m->b*tmp.c;
        p->b  = m->a*tmp.b  + m->b*tmp.d;
        p->c  = m->c*tmp.a  + m->d*tmp.c;
        p->d  = m->c*tmp.b  + m->d*tmp.d;
        p->tx = m->tx*tmp.a + m->ty*tmp.c + tmp.tx;
        p->ty = m->tx*tmp.b + m->ty*tmp.d + tmp.ty;
    }
}

void TkPathBeginPath(Drawable d, Tk_PathStyle *style)
{
    /* empty */
}

void TkPathMoveTo(Drawable d, double x, double y)
{
    double *coordPtr;
    _PathSegments *segm, *currentSegm;

    segm = _NewPathSegments();
    if (gctx->segm == NULL) {
        gctx->segm = segm;
        gctx->currentSegm = segm;
    } else {
        currentSegm = gctx->currentSegm;
        currentSegm->next = segm;
        gctx->currentSegm = segm;
    }
    gctx->hasCurrent = 1;
    gctx->current[0] = x;
    gctx->current[1] = y;
    gctx->lastMove[0] = x;
    gctx->lastMove[1] = y;
    coordPtr = segm->points;
    PathApplyTMatrixToPoint(gctx->m, gctx->current, coordPtr);
    segm->npoints = 1;
}

void TkPathLineTo(Drawable d, double x, double y)
{
    double *coordPtr;
    _PathSegments *segm;
    
    segm = gctx->currentSegm;
    gctx->current[0] = x;
    gctx->current[1] = y;
    coordPtr = segm->points + 2*segm->npoints;
    PathApplyTMatrixToPoint(gctx->m, gctx->current, coordPtr);    
    (segm->npoints)++;
}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double cx, cy;
    double x31, y31, x32, y32;
    
    cx = gctx->current[0];
    cy = gctx->current[1];

    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = cx + (ctrlX - cx) * 2 / 3;
    y31 = cy + (ctrlY - cy) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;

    TkPathCurveTo(d, x31, y31, x32, y32, x, y);
}

void TkPathCurveTo(Drawable d, double x1, double y1, 
        double x2, double y2, double x, double y)
{
    int numSteps;
    double *coordPtr;
    double control[8];
    double xc, yc;
    _PathSegments *segm;

    xc = x;
    yc = y;

    PathApplyTMatrixToPoint(gctx->m, gctx->current, control);
    PathApplyTMatrix(gctx->m, &x1, &y1);
    PathApplyTMatrix(gctx->m, &x2, &y2);
    PathApplyTMatrix(gctx->m, &x, &y);
    control[2] = x1;
    control[3] = y1;
    control[4] = x2;
    control[5] = y2;
    control[6] = x;
    control[7] = y;

    numSteps = kPathNumSegmentsCurveTo;
    segm = gctx->currentSegm;
    coordPtr = segm->points + 2*segm->npoints;
    CurveSegments(control, 0, numSteps, coordPtr);
    segm->npoints += numSteps;
    gctx->current[0] = xc;
    gctx->current[1] = yc;
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
    double *coordPtr;
    _PathSegments *segm;

    segm = gctx->currentSegm;
    segm->isclosed = 1;
    gctx->current[0] = gctx->lastMove[0];
    gctx->current[1] = gctx->lastMove[1];
    coordPtr = segm->points + 2*segm->npoints;
    PathApplyTMatrixToPoint(gctx->m, gctx->current, coordPtr);    
    (segm->npoints)++;
}

void TkPathClipToPath(Drawable d, int fillRule)
{
    /* empty */
}

void TkPathReleaseClipToPath(Drawable d)
{
    /* empty */
}

/* @@@ This is a very much simplified version of TkCanvTranslatePath that
 * doesn't do any clipping and no translation since we do that with
 * the more general affine matrix transform.
 */
static void _DoubleCoordsToXPointArray(int npoints, double *coordArr, XPoint *outArr)
{
    int i;
    double x, y;
    
    for(i = 0; i < npoints; i++){
        x = coordArr[i*2];
        y = coordArr[i*2+1];
    
        if (x > 0) {
            x += 0.5;
        } else {
            x -= 0.5;
        }
        outArr[i].x = (short) x;
    
        if (y > 0) {
            y += 0.5;
        } else {
            y -= 0.5;
        }
        outArr[i].y = (short) y;
    }
}

void TkPathStroke(Drawable d, Tk_PathStyle *style)
{       
    int numPoints;
    XPoint *pointPtr;
    _PathSegments *segm;
    
    segm = gctx->segm;
    while (segm != NULL) {
        numPoints = segm->npoints;
        pointPtr = (XPoint *)ckalloc((unsigned)(numPoints * sizeof(XPoint)));
        _DoubleCoordsToXPointArray(numPoints, segm->points, pointPtr);
        XDrawLines(gctx->display, gctx->drawable, style->strokeGC, pointPtr, numPoints,
                CoordModeOrigin);
        ckfree((char *) pointPtr);
        segm = segm->next;
    }
}

void TkPathFill(Drawable d, Tk_PathStyle *style)
{
    int numPoints;
    XPoint *pointPtr;
    _PathSegments *segm;
    
    segm = gctx->segm;
    while (segm != NULL) {
        numPoints = segm->npoints;
        pointPtr = (XPoint *)ckalloc((unsigned)(numPoints * sizeof(XPoint)));
        _DoubleCoordsToXPointArray(numPoints, segm->points, pointPtr);
        XFillPolygon(gctx->display, gctx->drawable, style->fillGC, pointPtr, numPoints,
                Complex, CoordModeOrigin);
        ckfree((char *) pointPtr);
        segm = segm->next;
    }
}

void TkPathFillAndStroke(Drawable d, Tk_PathStyle *style)
{
    int numPoints;
    XPoint *pointPtr;
    _PathSegments *segm;
    
    segm = gctx->segm;
    while (segm != NULL) {
        numPoints = segm->npoints;
        pointPtr = (XPoint *)ckalloc((unsigned)(numPoints * sizeof(XPoint)));
        _DoubleCoordsToXPointArray(numPoints, segm->points, pointPtr);
        XFillPolygon(gctx->display, gctx->drawable, style->fillGC, pointPtr, numPoints,
                Complex, CoordModeOrigin);
        XDrawLines(gctx->display, gctx->drawable, style->strokeGC, pointPtr, numPoints,
                CoordModeOrigin);
        ckfree((char *) pointPtr);
        segm = segm->next;
    }
}

void TkPathEndPath(Drawable d)
{
    /* empty */
}

void TkPathFree(Drawable d)
{
    _PathContextFree(gctx);
    gctx = NULL;
}

int TkPathDrawingDestroysPath(void)
{
    return 0;
}

int TkPathGetCurrentPosition(Drawable d, PathPoint *pt)
{
    pt->x = gctx->current[0];
    pt->y = gctx->current[1];
    return TCL_OK;
}

int TkPathBoundingBox(PathRect *rPtr)
{
    return TCL_ERROR;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{    
    /* The Tk X11 compatibility layer does not have tha ability to set up
     * clipping to pixmap which is needed here, I believe. 
     */
}
            

