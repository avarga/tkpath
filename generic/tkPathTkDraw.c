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


extern int gUseAntiAlias;

typedef struct _PathSegments {
    int 	npoints;
    double 	*points;
    struct _PathSegments *next;
} _PathSegments;

typedef struct _PathContext {
    double 	currentX;
    double	currentY;
    double 	lastMoveX;
    double	lastMoveY;
    int		hasCurrent;
    TMatrix *m;
    _PathSegments *segm;
    _PathSegments *currentSegm;
} _PathContext;

static _PathContext *gctx = NULL;

static _PathContext* _NewPathContext(void)
{
    _PathContext *ctx;
    
    ctx = (_PathContext *) ckalloc(sizeof(_PathContext));
    ctx->currentX = 0.0;
    ctx->currentY = 0.0;
    ctx->lastMoveX = 0.0;
    ctx->lastMoveY = 0.0;
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
    ckfree((char *) ctx);
}

void TkPathInit(Display *display, Drawable d)
{
    gctx = _NewPathContext();
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
    _PathSegments *segm, *currentSegm;

    segm = _NewPathSegments();
    if (gctx->segm == NULL) {
        gctx->segm = segm;
    } else {
        currentSegm = gctx->currentSegm;
        
        
    }
    
}

void TkPathLineTo(Drawable d, double x, double y)
{
    _PathSegments *segm;
    
    gctx->currentX = x;
    gctx->currentY = y;
    segm = gctx->currentSegm;
    if (gctx->m != NULL) {
        PathApplyTMatrix(gctx->m, &x, &y);
    }
    
}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double cx, cy;
    double x31, y31, x32, y32;
    
    cx = gctx->currentX;
    cy = gctx->currentY;

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
    /* empty */
}

void TkPathReleaseClipToPath(Drawable d)
{
    /* empty */
}

void TkPathStroke(Drawable d, Tk_PathStyle *style)
{       

    XDrawLines();
}

void TkPathFill(Drawable d, Tk_PathStyle *style)
{

    XFillPolygon();
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
    _PathContextFree(gctx);
    gctx = NULL;
}

int TkPathDrawingDestroysPath(void)
{
    return 0;
}

int TkPathGetCurrentPosition(Drawable d, PathPoint *pt)
{
    pt->x = gctx->currentX;
    pt->y = gctx->currentY;
    return TCL_OK;
}

int TkPathBoundingBox(PathRect *rPtr)
{
    return TCL_ERROR;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{    

}
            

