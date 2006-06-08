/*
 * tkUnixCairoPath.c --
 *
 *	This file implements path drawing API's using the Cairo rendering engine.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * $Id$
 */

#include <cairo.h>
#include <cairo-xlib.h>
#include <tkUnixInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

#define BlueDoubleFromXColorPtr(xc)   (double) (((xc)->pixel & 0xFF)) / 255.0
#define GreenDoubleFromXColorPtr(xc)  (double) ((((xc)->pixel >> 8) & 0xFF)) / 255.0
#define RedDoubleFromXColorPtr(xc)    (double) ((((xc)->pixel >> 16) & 0xFF)) / 255.0

extern int gUseAntiAlias;

/*
 * This is used as a place holder for platform dependent stuff between each call.
 */
typedef struct TkPathContext_ {
    Drawable 			d;
    cairo_t*	 		c;
    cairo_surface_t* 	surface;
} TkPathContext_;


TkPathContext TkPathInit(Tk_Window tkwin, Drawable d)
{
    cairo_t *c;
    cairo_surface_t *surface;
    TkPathContext_ *context = (TkPathContext_ *) ckalloc((unsigned) (sizeof(TkPathContext_)));
    surface = cairo_xlib_surface_create(Tk_Display(tkwin), d, Tk_Visual(tkwin), Tk_Width(tkwin), Tk_ReqHeight(tkwin));
    c = cairo_create(surface);
    context->c = c;
    context->d = d;
    context->surface = surface;
    return (TkPathContext) context;
}

void
TkPathPushTMatrix(TkPathContext ctx, TMatrix *m)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix, m->a, m->b, m->c, m->d, m->tx, m->ty);
    cairo_transform(context->c, &matrix);
}

void TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_new_path(context->c);
}

void TkPathMoveTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_move_to(context->c, x, y);
}

void TkPathLineTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_line_to(context->c, x, y);
}

void TkPathQuadBezier(TkPathContext ctx, double ctrlX, double ctrlY, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    double cx, cy;
    double x31, y31, x32, y32;
    
    cairo_get_current_point(context->c, &cx, &cy);

    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = cx + (ctrlX - cx) * 2 / 3;
    y31 = cy + (ctrlY - cy) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;

    cairo_curve_to(context->c, x31, y31, x32, y32, x, y);
}

void TkPathCurveTo(TkPathContext ctx, double x1, double y1, 
        double x2, double y2, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_curve_to(context->c, x1, y1, x2, y2, x, y);
}

void TkPathArcTo(TkPathContext ctx,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x, double y)
{
    TkPathArcToUsingBezier(ctx, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void
TkPathRect(TkPathContext ctx, double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_rectangle(context->c, x, y, width, height);
}

void
TkPathOval(TkPathContext ctx, double cx, double cy, double rx, double ry)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    if (rx == ry) {
        cairo_move_to(context->c, cx+rx, cy);
        cairo_arc(context->c, cx, cy, rx, 0.0, 2*M_PI);
        cairo_close_path(context->c);
    } else {
        cairo_save(context->c);
        cairo_translate(context->c, cx, cy);
        cairo_scale(context->c, rx, ry);
        cairo_move_to(context->c, 1.0, 0.0);
        cairo_arc(context->c, 0.0, 0.0, 1.0, 0.0, 2*M_PI);
        cairo_close_path(context->c);
        cairo_restore(context->c);
    }
}

void
TkPathImage(TkPathContext ctx, Tk_PhotoHandle photo, double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    Tk_PhotoImageBlock block;
    cairo_surface_t *surface;
    cairo_format_t format;
    unsigned char *data;
    int size;

    /* Return value? */
    Tk_PhotoGetImage(photo, &block);
    size = block.pitch * block.height;
    if (width == 0.0) {
        width = (double) block.width;
    }
    if (height == 0.0) {
        height = (double) block.height;
    }
    
    /*
     * @format: the format of pixels in the buffer
     * @width: the width of the image to be stored in the buffer
     * @height: the eight of the image to be stored in the buffer
     * @stride: the number of bytes between the start of rows
     *   in the buffer. Having this be specified separate from @width
     *   allows for padding at the end of rows, or for writing
     *   to a subportion of a larger image.
     */
    if (block.pixelSize*8 == 32) {
        if (block.offset[3] == 3) {
            /* This is real fake! But cairo has no RGBA format. */
            format = CAIRO_FORMAT_ARGB32;
            //format = ???;
            data = (unsigned char *) (block.pixelPtr) + 0;
        } else if (block.offset[3] == 0) {
            format = CAIRO_FORMAT_ARGB32;
            data = (unsigned char *) (block.pixelPtr);
        } else {
            /* @@@ What to do here? */
            return;
        }
    }
    surface = cairo_image_surface_create_for_data(
            data,
            format, 
            (int) width, (int) height, 
            block.pitch);		/* stride */
    cairo_set_source_surface(context->c, surface, x, y);
    cairo_paint(context->c);
}

void TkPathClosePath(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_close_path(context->c);
}

void TkPathClipToPath(TkPathContext ctx, int fillRule)
{
    /* Clipping to path is done by default. */
    /* Note: cairo_clip does not consume the current path */
    //cairo_clip(context->c);
}

void TkPathReleaseClipToPath(TkPathContext ctx)
{
    //cairo_reset_clip(context->c);
}

void TkPathStroke(TkPathContext ctx, Tk_PathStyle *style)
{       
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    Tk_Dash *dash;

    cairo_set_source_rgba(context->c,             
            RedDoubleFromXColorPtr(style->strokeColor),
            GreenDoubleFromXColorPtr(style->strokeColor),
            BlueDoubleFromXColorPtr(style->strokeColor), 
            style->strokeOpacity);
    cairo_set_line_width(context->c, style->strokeWidth);

    switch (style->capStyle) {
        case CapNotLast:
        case CapButt:
            cairo_set_line_cap(context->c, CAIRO_LINE_CAP_BUTT);
            break;
        case CapRound:
            cairo_set_line_cap(context->c, CAIRO_LINE_CAP_ROUND);
            break;
        default:
            cairo_set_line_cap(context->c, CAIRO_LINE_CAP_SQUARE);
            break;
    }
    switch (style->joinStyle) {
        case JoinMiter: 
            cairo_set_line_join(context->c, CAIRO_LINE_JOIN_MITER);
            break;
        case JoinRound:
            cairo_set_line_join(context->c, CAIRO_LINE_JOIN_ROUND);
            break;
        default:
            cairo_set_line_join(context->c, CAIRO_LINE_JOIN_BEVEL);
            break;
    }
    cairo_set_miter_limit(context->c, style->miterLimit);

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
            cairo_set_dash(context->c, dashes, len, style->offset);
            ckfree((char *) dashes);
            ckfree((char *) array);
        }
    }

    cairo_stroke(context->c);
}

void CairoSetFill(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_set_source_rgba(context->c,
            RedDoubleFromXColorPtr(style->fillColor),
            GreenDoubleFromXColorPtr(style->fillColor),
            BlueDoubleFromXColorPtr(style->fillColor),
            style->fillOpacity);
    cairo_set_fill_rule(context->c, 
            (style->fillRule == WindingRule) ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
}

void TkPathFill(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CairoSetFill(ctx, style);
    cairo_fill(context->c);
}

void TkPathFillAndStroke(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CairoSetFill(ctx, style);
    cairo_fill_preserve(context->c);
    TkPathStroke(ctx, style);
}

void TkPathEndPath(TkPathContext ctx)
{
    /* Empty ??? */
}

void TkPathFree(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_destroy(context->c);
    cairo_surface_destroy(context->surface);
    ckfree((char *) context);
}

int TkPathDrawingDestroysPath(void)
{
    return 1;
}

int TkPathGetCurrentPosition(TkPathContext ctx, PathPoint *pt)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    cairo_get_current_point(context->c, &(pt->x), &(pt->y));
    return TCL_OK;
}

int TkPathBoundingBox(TkPathContext ctx, PathRect *rPtr)
{
    return TCL_ERROR;
}

void TkPathPaintLinearGradient(TkPathContext ctx, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{    
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    int					i;
    int					nstops;
    int					fillMethod;
    double				x1, y1, x2, y2;
    PathRect 			transition;		/* The transition line. */
    GradientStop 		*stop;
    cairo_pattern_t 	*pattern;
    cairo_extend_t		extend;
    
    /*
     * The current path is consumed by filling.
     * Need therfore to save the current context and restore after.
     */
    cairo_save(context->c);

    transition = fillPtr->transition;
    nstops = fillPtr->nstops;
    fillMethod = fillPtr->method;
    
    /* Scale up 'transition' vector to bbox. */
    x1 = bbox->x1 + (bbox->x2 - bbox->x1)*transition.x1;
    y1 = bbox->y1 + (bbox->y2 - bbox->y1)*transition.y1;
    x2 = bbox->x1 + (bbox->x2 - bbox->x1)*transition.x2;
    y2 = bbox->y1 + (bbox->y2 - bbox->y1)*transition.y2;

    pattern = cairo_pattern_create_linear(x1, y1, x2, y2);
    for (i = 0; i < nstops; i++) {
        stop = fillPtr->stops[i];
        cairo_pattern_add_color_stop_rgba (pattern, stop->offset, 
                RedDoubleFromXColorPtr(stop->color),
                GreenDoubleFromXColorPtr(stop->color),
                BlueDoubleFromXColorPtr(stop->color),
                stop->opacity);
    }
    cairo_set_source(context->c, pattern);

    cairo_set_fill_rule(context->c, 
            (fillRule == WindingRule) ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
            
    switch (fillMethod) {
        case kPathGradientMethodPad: 
            extend = CAIRO_EXTEND_NONE;
            break;
        case kPathGradientMethodRepeat:
            extend = CAIRO_EXTEND_REPEAT;
            break;
        case kPathGradientMethodReflect:
            extend = CAIRO_EXTEND_REFLECT;
            break;
        default:
            extend = CAIRO_EXTEND_NONE;
            break;
    }
    cairo_pattern_set_extend(pattern, extend);
    cairo_fill(context->c);
    
    cairo_pattern_destroy(pattern);
    cairo_restore(context->c);
}
            
