/*
 * tkUnixCairoPath.c --
 *
 *	This file implements path drawing API's using the Cairo rendering engine.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

/* Never built this one. Just "random" code! */

/*** Help yourself!!! ***/

#include <cairo.h>
#include <tkUnixInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

#define BlueDoubleFromXColorPtr(xc)   (double) (((xc)->pixel & 0xFF)) / 255.0
#define GreenDoubleFromXColorPtr(xc)  (double) ((((xc)->pixel >> 8) & 0xFF)) / 255.0
#define RedDoubleFromXColorPtr(xc)    (double) ((((xc)->pixel >> 16) & 0xFF)) / 255.0

extern int gUseAntialiasing;

static  cairo_t *gctx = NULL;

void TkPathInit(Drawable d)
{
    if (gctx != NULL) {
	Tcl_Panic("the path drawing context gctx is already in use\n");
    }
    gctx = cairo_create();
    cairo_set_target_drawable(gctx,
            GDK_WINDOW_XDISPLAY(d),
            GDK_WINDOW_XWINDOW(d));
}

void
TkPathPushTMatrix(Drawable d, TMatrix *m)
{
    cairo_matrix_t *matrix = cairo_matrix_create();
    cairo_matrix_set_affine(matrix, m->a, m->b, m->c, m->d, m->tx, m->ty);
    cairo_concat_matrix(gctx, matrix);
}

void TkPathBeginPath(Drawable d, Tk_PathStyle *style)
{

    /* save/pop the state so we don't screw up the xform */
    //cairo_save(gctx);
    cairo_new_path(gctx);
}

void TkPathMoveTo(Drawable d, double x, double y)
{
    cairo_move_to(gctx, x, y);
}

void TkPathLineTo(Drawable d, double x, double y)
{
    cairo_line_to(gctx, (float) x, (float) y);
}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double cx, cy;
    double x31, y31, x32, y32;
    
    cairo_current_point(gctx, &cx, &cy);

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
    cairo_curve_to(gctx, (float) x1, (float) y1, (float) x2, (float) y2, (float) x, (float) y);
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
    cairo_close_path(gctx);
}

void TkPathClipToPath(Drawable d, int fillRule)
{

}

void TkPathReleaseClipToPath(Drawable d)
{

}

void TkPathStroke(Drawable d, Tk_PathStyle *style)
{       
    Tk_Dash *dash;

    cairo_set_rgb_color(ctx,
            RedDoubleFromXColorPtr(style->strokeColor),
            GreenDoubleFromXColorPtr(style->strokeColor),
            BlueDoubleFromXColorPtr(style->strokeColor));
    cairo_set_alpha(ctx, double(opacity));
    cairo_set_line_width(ctx, style->strokeWidth);

    switch (stylePtr->capStyle) {
        case CapNotLast:
        case CapButt:
            cairo_set_line_cap(gctx, CAIRO_LINE_CAP_BUTT);
            break;
        case CapRound:
            cairo_set_line_cap(gctx, CAIRO_LINE_CAP_ROUND);
            break;
        default:
            cairo_set_line_cap(gctx, CAIRO_LINE_CAP_SQUARE);
            break;
    }
    switch (stylePtr->joinStyle) {
        case JoinMiter: 
            cairo_set_line_join(gctx, CAIRO_LINE_JOIN_MITER);
            break;
        case JoinRound:
            cairo_set_line_join(gctx, CAIRO_LINE_JOIN_ROUND);
            break;
        default:
            cairo_set_line_join(gctx, CAIRO_LINE_JOIN_BEVEL);
            break;
    }
    cairo_set_miter_limit(gctx, style.miterLimit);

    dash = &(style->dash);
    if ((dash != NULL) && (dash->number != 0)) {
        int	i, len;
        float 	*array;
    
        PathParseDashToArray(dash, style->strokeWidth, &len, &array);
        if (len > 0) {
            double *dashes = (double *) ckalloc(len*sizeof(double));

            for (i = 0; i < len; i++) {
                dashes[i] = array[i];
            }
            cairo_set_dash(gctx, dashes, len, style->offset);
            ckfree((char *) dashes);
            ckfree((char *) array);
        }
    }

    cairo_stroke(gctx);
}

void TkPathFill(Drawable d, Tk_PathStyle *style)
{
    if (style->stroke != NULL) {
        cairo_save(gctx);
    }
    cairo_set_rgb_color(gctx,
            RedDoubleFromXColorPtr(style->fillColor),
            GreenDoubleFromXColorPtr(style->fillColor),
            BlueDoubleFromXColorPtr(style->fillColor));
    cairo_set_alpha(gctx, style->fillOpacity);
    cairo_set_fill_rule(gctx, 
            (style->fillRule == WindingRule) ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill(gctx);
    if (style->stroke != NULL) {
        cairo_restore(ctx);
    }
}

void TkPathFillAndStroke(Drawable d, Tk_PathStyle *style)
{
    TkPathFill(d, style);
    TkPathStroke(d, style);
}

void TkPathEndPath(Drawable d)
{
    /* Empty ??? */
}

void TkPathFree(Drawable d)
{
    cairo_destroy(gctx);
    gctx = NULL;
}

int TkPathDrawingDestroysPath(void)
{
    return 0;	//???
}

int TkPathGetCurrentPosition(Drawable d, PathPoint *pt)
{
    cairo_current_point(gctx, &(pt->x), &(pt->y));
    return TCL_OK;
}

int TkPathBoundingBox(PathRect *rPtr)
{
    return TCL_ERROR;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{

}


