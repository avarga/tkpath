/*
 * tkWinGDIPath.c --
 *
 *		This file implements path drawing API's on Windows using the GDI lib.
 *  	GDI is missing several features found in GDI+.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>
#include <tkWinInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

#define Red255FromXColorPtr(xc)    (((xc)->pixel) & 0xFF)
#define Green255FromXColorPtr(xc)  (((xc)->pixel >> 8) & 0xFF)
#define Blue255FromXColorPtr(xc)   (((xc)->pixel >> 16) & 0xFF)

extern int gUseAntiAlias;

/*
 * This is perhaps a very stupid thing to do.
 * It limits drawing to this single context at a time.
 * Perhaps some window structure should be augmented???
 */

static HDC gMemHdc = NULL;
static TMatrix gCTM;		/* The context transformation matrix. */
static int gHaveMatrix;

/*
 * Since we do the coordinate transforms ourself we cannot rely
 * on the GDI api to get current untransformed point.
 */
 
static double gCurrentX;
static double gCurrentY;

static HPEN
PathCreatePen(Tk_PathStyle *stylePtr)
{
    int widthInt;
    int useDash = 0;
    int	dashLen = 0;
    int win2000atleast = 0;
    Tk_Dash *dash;
    OSVERSIONINFO os;
    DWORD penstyle;
    HPEN hpen;
    DWORD *lpStyle = NULL;
    
    /* Windows 95/98/Me need special handling since
     * they do not support dashes through the custom style array.
     * This is the two last arguments of ExtCreatePen().
     * Therefore all dashing is switched off for these OSes.
     * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/pens_6rse.asp 
     */

    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
    if ((os.dwPlatformId == VER_PLATFORM_WIN32_NT) && (os.dwMajorVersion >= 5)) {
        win2000atleast = 1;
    }
    widthInt = (int) (stylePtr->strokeWidth + 0.5);
    if (widthInt < 1) {
        widthInt = 1; 	// @@@ ????
    }

    /* Set the line dash pattern in the current graphics state. */
    dash = &(stylePtr->dash);
    if (win2000atleast && (dash != NULL) && (dash->number != 0)) {
        useDash = 1;
    }

    /* "A cosmetic pen can only be a single pixel wide and must be a solid color, 
     * but cosmetic pens are generally faster than geometric pens." 
     */
    if ((widthInt <= 1) && !useDash) {
        penstyle = PS_COSMETIC;
    } else {
        penstyle = PS_GEOMETRIC;
    }
    if (useDash) {
        int i;
        float *dashArray = NULL;
        
        PathParseDashToArray(dash, stylePtr->strokeWidth, &dashLen, &dashArray);
        if (dashLen > 0) {
            lpStyle = (DWORD *) ckalloc(dashLen*sizeof(DWORD));
            for (i = 0; i < dashLen; i++) {
                lpStyle[i] = (DWORD) (dashArray[i] + 0.5);
            }
        }
        if (dashArray != NULL) {
            ckfree((char *) dashArray);
        }
        penstyle |= PS_USERSTYLE;
    } else {
        penstyle |= PS_SOLID;
    }
    if (stylePtr->strokeStipple != None) {
        /* @@@ TODO */
    }
    if (penstyle & PS_COSMETIC) {
        hpen = CreatePen(penstyle, widthInt, stylePtr->strokeColor->pixel);
    } else {
        LOGBRUSH lb;
    
        lb.lbStyle = BS_SOLID;
        lb.lbColor = stylePtr->strokeColor->pixel;
        lb.lbHatch = 0;
    
        if (widthInt >= 3) {
            switch (stylePtr->capStyle) {
                case CapNotLast:
                case CapButt:
                    penstyle |= PS_ENDCAP_FLAT; 
                    break;
                case CapRound:
                    penstyle |= PS_ENDCAP_ROUND; 
                    break;
                default:
                    penstyle |= PS_ENDCAP_SQUARE; 
                    break;
            }
            switch (stylePtr->joinStyle) {
                case JoinMiter: 
                    penstyle |= PS_JOIN_MITER; 
                    break;
                case JoinRound:
                    penstyle |= PS_JOIN_ROUND; 
                    break;
                default:
                    penstyle |= PS_JOIN_BEVEL; 
                    break;
            }
        }
        hpen = ExtCreatePen(penstyle, widthInt, &lb, (DWORD) dashLen, lpStyle);
    }
    return hpen;
}

static HBRUSH
PathCreateBrush(Tk_PathStyle *style)
{
    if (style->fillColor != NULL) {
        return CreateSolidBrush(style->fillColor->pixel);
    } else {
        return NULL;
    }
    if (style->fillStipple != None) {
        /* @@@ TODO */
    }
}

void		
TkPathInit(Drawable d)
{
    HDC hdc;
    TkWinDrawable *twdPtr = (TkWinDrawable *)d;

    if (gMemHdc != NULL) {
        Tcl_Panic("the path drawing context gMemHdc is already in use\n");
    }

    /* This will only work for bitmaps; need something else! TkWinGetDrawableDC()? */
    hdc = CreateCompatibleDC(NULL);
    SelectObject(hdc, twdPtr->bitmap.handle);
    gMemHdc = hdc;
    gHaveMatrix = 0;
    gCTM = kUnitTMatrix;
    gCurrentX = 0.0;
    gCurrentY = 0.0;
}

void
TkPathPushTMatrix(Drawable d, TMatrix *m)
{
    TMatrix tmp = gCTM;
    
    gHaveMatrix = 1;
    gCTM.a  = m->a*tmp.a  + m->b*tmp.c;
    gCTM.b  = m->a*tmp.b  + m->b*tmp.d;
    gCTM.c  = m->c*tmp.a  + m->d*tmp.c;
    gCTM.d  = m->c*tmp.b  + m->d*tmp.d;
    gCTM.tx = m->tx*tmp.a + m->ty*tmp.c + tmp.tx;
    gCTM.ty = m->tx*tmp.b + m->ty*tmp.d + tmp.ty;
}

void
TkPathBeginPath(Drawable d, Tk_PathStyle *stylePtr)
{
    BeginPath(gMemHdc);
}

void
TkPathMoveTo(Drawable d, double x, double y)
{    
    gCurrentX = x;
    gCurrentY = y;
    if (gHaveMatrix) {
        PathApplyTMatrix(&gCTM, &x, &y);
    }
    MoveToEx(gMemHdc, (int) (x + 0.5), (int) (y + 0.5), (LPPOINT) NULL);
}

void
TkPathLineTo(Drawable d, double x, double y)
{
    gCurrentX = x;
    gCurrentY = y;
    if (gHaveMatrix) {
        PathApplyTMatrix(&gCTM, &x, &y);
    }
    LineTo(gMemHdc, (int) (x + 0.5), (int) (y + 0.5));
}

void
TkPathLinesTo(Drawable d, double *pts, int n)
{

    //Polyline(gMemHdc, );
}

void
TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    POINT ptc;		/* Current point. */
    POINT pts[3];	/* Control points. */
    
    gCurrentX = x;
    gCurrentY = y;

    /* Current in transformed coords. */
    GetCurrentPositionEx(gMemHdc, &ptc);

    if (gHaveMatrix) {
        PathApplyTMatrix(&gCTM, &ctrlX, &ctrlY);
        PathApplyTMatrix(&gCTM, &x, &y);
    }    

    /* Conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg) */
    /* Unchecked! Must be an approximation! */

    pts[0].x = (LONG) (ptc.x + (ctrlX - ptc.x) * 2 / 3 + 0.5);
    pts[0].y = (LONG) (ptc.y + (ctrlY - ptc.y) * 2 / 3 + 0.5);
    pts[1].x = (LONG) (ctrlX + (x - ctrlX) / 3 + 0.5);
    pts[1].y = (LONG) (ctrlY + (y - ctrlY) / 3 + 0.5);
    pts[2].x = (LONG) (x + 0.5);
    pts[2].y = (LONG) (y + 0.5);
    PolyBezierTo(gMemHdc, pts, 3);
}

void
TkPathCurveTo(Drawable d, double ctrlX1, double ctrlY1, 
        double ctrlX2, double ctrlY2, double x, double y)
{
    POINT pts[3];

    gCurrentX = x;
    gCurrentY = y;
    if (gHaveMatrix) {
        PathApplyTMatrix(&gCTM, &ctrlX1, &ctrlY1);
        PathApplyTMatrix(&gCTM, &ctrlX2, &ctrlY2);
        PathApplyTMatrix(&gCTM, &x, &y);
    }    
    pts[0].x = (LONG) (ctrlX1 + 0.5);
    pts[0].y = (LONG) (ctrlY1 + 0.5);
    pts[1].x = (LONG) (ctrlX2 + 0.5);
    pts[1].y = (LONG) (ctrlY2 + 0.5);
    pts[2].x = (LONG) (x + 0.5);
    pts[2].y = (LONG) (y + 0.5);
    PolyBezierTo(gMemHdc, pts, 3);
}

void
TkPathArcTo(Drawable d,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x2, double y2)
{
    TkPathArcToUsingBezier(d, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x2, y2);
}

void
TkPathClosePath(Drawable d)
{
    CloseFigure(gMemHdc);
}

void
TkPathEndPath(Drawable d)
{
    EndPath(gMemHdc);
}

void
TkPathFree(Drawable d)
{
    DeleteDC(gMemHdc);
    gMemHdc = NULL;
}

void		
TkPathClipToPath(Drawable d, int fillRule)
{
    SelectClipPath(gMemHdc, RGN_AND);
}

void
TkPathReleaseClipToPath(Drawable d)
{
    SelectClipRgn(gMemHdc, NULL);
}

void
TkPathStroke(Drawable d, Tk_PathStyle *style)
{       
    HDC hdc;
    HPEN hpen, oldHpen;
    
    hdc = gMemHdc;
    hpen = PathCreatePen(style);
    oldHpen = SelectObject(hdc, hpen);
    SetBkMode(hdc, TRANSPARENT);
    SetMiterLimit(hdc, (FLOAT) style->miterLimit, NULL);
    
    StrokePath(hdc);
    
    DeleteObject(SelectObject(hdc, oldHpen));
}

void
TkPathFill(Drawable d, Tk_PathStyle *style)
{
    HDC hdc;
    HBRUSH hbrush, oldHbrush;
    
    hdc = gMemHdc;
    hbrush = PathCreateBrush(style);
	oldHbrush = SelectObject(hdc, hbrush);
    SetPolyFillMode(hdc, (style->fillRule == WindingRule) ? WINDING : ALTERNATE);

    FillPath(hdc);

    DeleteObject(SelectObject(hdc, oldHbrush));
}

void        
TkPathFillAndStroke(Drawable d, Tk_PathStyle *style)
{
    HDC hdc;
    HPEN hpen, oldHpen;
    HBRUSH hbrush, oldHbrush;
    
    hdc = gMemHdc;
    hpen = PathCreatePen(style);
    hbrush = PathCreateBrush(style);
    oldHpen = SelectObject(hdc, hpen);
	oldHbrush = SelectObject(hdc, hbrush);
    SetBkMode(hdc, TRANSPARENT);
    SetPolyFillMode(hdc, (style->fillRule == WindingRule) ? WINDING : ALTERNATE);
    SetMiterLimit(hdc, (FLOAT) style->miterLimit, NULL);

    StrokeAndFillPath(hdc);

    DeleteObject(SelectObject(hdc, oldHpen));
    DeleteObject(SelectObject(hdc, oldHbrush));
}

/* TkPathGetCurrentPosition --
 *
 * 		Returns the current pen position in untransformed coordinates!
 */

int
TkPathGetCurrentPosition(Drawable d, PathPoint *ptPtr)
{
    //POINT pt;
    
    //GetCurrentPositionEx(gMemHdc, &pt);
    ptPtr->x = gCurrentX;
    ptPtr->y = gCurrentY;
    return TCL_OK;
}

int		
TkPathDrawingDestroysPath(void)
{
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TwoStopLinearGradient --
 *
 *		Paint linear gradients using primitive drawing functions.
 *
 *----------------------------------------------------------------------
 */

static void
TwoStopLinearGradient(
        Drawable d, 
        PathRect *bbox, /* The items bounding box in untransformed coords. */
        PathRect *line,	/* The relative line that defines the
                         * gradient transition line. 
                         * We paint perp to this line. */
        XColor *color1, XColor *color2, double opacity1, double opacity2)
{
    int i, nsteps;
    int widthInt, penWidth;
    int rgba1[4], rgba2[4], deltaRGBA[4];
    int maxDeltaRGBA;
    int red, green, blue;
    double width, relative;
    double ax, ay;
    double a2, alen;
    double p1x, p1y, p2x, p2y;
    double diag;
    double q1x, q1y, q2x, q2y;
    double x1, y1, x2, y2;
    double deltaX, deltaY;
    HPEN hpen;

    /* Scale up 'line' vector to bbox. */
    p1x = bbox->x1 + (bbox->x2 - bbox->x1)*line->x1;
    p1y = bbox->y1 + (bbox->y2 - bbox->y1)*line->y1;
    p2x = bbox->x1 + (bbox->x2 - bbox->x1)*line->x2;
    p2y = bbox->y1 + (bbox->y2 - bbox->y1)*line->y2;

    /*
     * Vectors: 
     *		p1 = start transition vector
     *		p2 = end transition vector
     *		a  = P2 - P1
     */
    ax = p2x - p1x;
    ay = p2y - p1y;
    a2 = ax*ax + ay*ay;
    if (a2 < 0.5) {
        /* Not much to paint in this case. */
        return;
    }
    alen = sqrt(a2);

    /*
     * Perp translate 'a' to left and right so that they "cover" the bbox.
     * Vectors: q1 = p1 + (-ay, ax)*diag/|a|
     *          q2 = p1 + (ay, -ax)*dist/|a|
     */
    diag = hypot(bbox->x2 - bbox->x1, bbox->y2 - bbox->y1) + 2.0;
    q1x = p1x - ay*diag/alen;
    q1y = p1y + ax*diag/alen;
    q2x = p1x + ay*diag/alen;
    q2y = p1y - ax*diag/alen;
    
    /*
     * So far everything has taken place in the untransformed
     * coordinate system.
     */  
    if (gHaveMatrix) {
        /*
         * This is not a 100% foolproof solution. Tricky!
         */
        PathApplyTMatrix(&gCTM, &q1x, &q1y);
        PathApplyTMatrix(&gCTM, &q2x, &q2y);
        PathApplyTMatrix(&gCTM, &p1x, &p1y);
        PathApplyTMatrix(&gCTM, &p2x, &p2y);
		ax = p2x - p1x;
	    ay = p2y - p1y;
        a2 = ax*ax + ay*ay;
        alen = sqrt(a2);
    }    
    
    /* Color differences. Opacity missing in GDI. */
    rgba1[0] = Red255FromXColorPtr(color1);
    rgba1[1] = Green255FromXColorPtr(color1);
    rgba1[2] = Blue255FromXColorPtr(color1);
    rgba2[0] = Red255FromXColorPtr(color2);
    rgba2[1] = Green255FromXColorPtr(color2);
    rgba2[2] = Blue255FromXColorPtr(color2);
    maxDeltaRGBA = 0;
    for (i = 0; i < 3; i++) {
        deltaRGBA[i] = rgba2[i] - rgba1[i];
        if (abs(deltaRGBA[i]) > maxDeltaRGBA) {
            maxDeltaRGBA = abs(deltaRGBA[i]);
        }
    }
    
    /* The width of lines to paint with and still
     * keep a smooth gradient. */
    if (maxDeltaRGBA <= 1) {
        width = alen;
        nsteps = 1;
    } else {
        width = alen/maxDeltaRGBA;
        nsteps = maxDeltaRGBA;
    }
    deltaX = width * ax/alen;
    deltaY = width * ay/alen;
    widthInt = (int) (width + 1.0);

    for (i = 0, x1 = q1x + deltaX/2.0, y1 = q1y + deltaY/2.0,
            x2 = q2x + deltaX/2.0, y2 = q2y + deltaY/2.0;
            i < nsteps; 
            i++, x1 += deltaX, y1 += deltaY, x2 += deltaX, y2 += deltaY) {
        relative = (double) i/(double) nsteps;
        red   = (int) (rgba1[0] + relative*deltaRGBA[0]);
        green = (int) (rgba1[1] + relative*deltaRGBA[1]);
        blue  = (int) (rgba1[2] + relative*deltaRGBA[2]);
        penWidth = widthInt;
        
        /* Verify that we don't paint outside. */
        
        
        hpen = SelectObject(gMemHdc, 
                CreatePen(PS_SOLID, penWidth, RGB(red, green, blue)));
        MoveToEx(gMemHdc, (int) (x1 + 0.5), (int) (y1 + 0.5), NULL);
        LineTo(gMemHdc, (int) (x2 + 0.5), (int) (y2 + 0.5));
        DeleteObject(SelectObject(gMemHdc, hpen));
    }
}

/*
 * This code is a bit too general to be placed in platform specific file,
 * but the problem is that setting the clip path resets the path
 * which therefore needs to be redefined. This is needed if we need to
 * stroke it after this. ??????????????
 */
 
#define FloatsEqual(a, b) ((fabs((a) - (b)) < 1e-6) ? 1 : 0)

enum {
    kPathGradientPadNone 	= 0L,
    kPathGradientPadFirst,
    kPathGradientPadSecond,
    kPathGradientPadBoth
};

/* Find out if a line defining a linear gradient requires padding at its ends. */

static int
NeedTransitionPadding(PathRect *line)
{
    int xAtBorders = 0, yAtBorders = 0;
    int padFirst = 0, padSecond = 0;
    double x1, y1, x2, y2;
    
    x1 = line->x1;
    y1 = line->y1;
    x2 = line->x2;
    y2 = line->y2;
    
    if ((FloatsEqual(x1, 0.0) && FloatsEqual(x2, 1.0)) ||
            (FloatsEqual(x1, 1.0) && FloatsEqual(x2, 0.0))) {
        xAtBorders = 1;
    }
    if ((FloatsEqual(y1, 0.0) && FloatsEqual(y2, 1.0)) ||
            (FloatsEqual(y1, 1.0) && FloatsEqual(y2, 0.0))) {
        yAtBorders = 1;
    }
    
    /* Line is diagonal in square. */
    if (xAtBorders && yAtBorders) {
        return kPathGradientPadNone;
    }

    /* From one side to another, parallell to either axes. */
    if (xAtBorders && FloatsEqual(y1, y2)) {
        return kPathGradientPadNone;
    }
    if (yAtBorders && FloatsEqual(x1, x2)) {
        return kPathGradientPadNone;
    }
    
    /* Pad first? */
    if (((x1 > 0.0) && (x2 > x1)) || ((x1 < 1.0) && (x2 < x1))) {
        padFirst = 1;
    }
    if (((y1 > 0.0) && (y2 > y1)) || ((y1 < 1.0) && (y2 < y1))) {
        padFirst = 1;
    }
    
    /* Pad second? */
    if (((x2 < 1.0) && (x2 > x1)) || ((x2 > 0.0) && (x2 < x1))) {
        padSecond = 1;
    }
    if (((y2 < 1.0) && (y2 > y1)) || ((y2 > 0.0) && (y2 < y1))) {
        padSecond = 1;
    }
    if (padFirst && padSecond) {
        return kPathGradientPadBoth;
    } else if (padFirst) {
        return kPathGradientPadFirst;
    } else if (padSecond) {
        return kPathGradientPadSecond;
    } else {
        return kPathGradientPadNone;
    }
}

void
TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    int i;
    int pad;
    int nstops;
    PathRect line, transition;
    GradientStop *stop1, *stop2;

    transition = fillPtr->transition;
    nstops = fillPtr->nstops;

    if (fillPtr->method == kPathGradientMethodPad) {
    
        if ((pad = NeedTransitionPadding(&transition)) != kPathGradientPadNone) {
        
            /* 
             * Construct the pad lines using worst case (diagonal; sqrt(2)):
             * If r1 and r2 are begin and end points of the transition vector,
             * then first pad line is {r1 - sqrt(2)*(r2 - r1), r1}. 
             * Second pad line is {r2, (r2 + sqrt(2)*(r2 - r1)}
             */
            if ((pad == kPathGradientPadFirst) || (pad == kPathGradientPadBoth)) {
                line.x1 = transition.x1 - 1.42*(transition.x2 - transition.x1);
                line.y1 = transition.y1 - 1.42*(transition.y2 - transition.y1);
                line.x2 = transition.x1;
                line.y2 = transition.y1;
                stop1 = fillPtr->stops[0];
                TwoStopLinearGradient(d, bbox, &line,
                        stop1->color, stop1->color, stop1->opacity, stop1->opacity);
            }
            if ((pad == kPathGradientPadSecond) || (pad == kPathGradientPadBoth)) {
                line.x1 = transition.x2;
                line.y1 = transition.y2;
                line.x2 = transition.x2 + 1.42*(transition.x2 - transition.x1);
                line.y2 = transition.y2 + 1.42*(transition.y2 - transition.y1);
                stop1 = fillPtr->stops[nstops - 1];
                TwoStopLinearGradient(d, bbox, &line,
                        stop1->color, stop1->color, stop1->opacity, stop1->opacity);
            }
        }
        
        /*
         * What happens if first offset > 0.0 or last offset < 1.0? Pad.
         */
        if (fillPtr->stops[0]->offset > 0.0) {
            stop1 = fillPtr->stops[0];
            line.x1 = transition.x1;
            line.y1 = transition.y1;
            line.x2 = transition.x1 + stop1->offset * (transition.x2 - transition.x1);
            line.y2 = transition.y1 + stop1->offset * (transition.y2 - transition.y1);            
            TwoStopLinearGradient(d, bbox, &line,
                    stop1->color, stop1->color, stop1->opacity, stop1->opacity);
        }
        if (fillPtr->stops[nstops-1]->offset < 1.0) {
            stop2 = fillPtr->stops[nstops-1];
            line.x1 = transition.x1 + stop2->offset * (transition.x2 - transition.x1);
            line.y1 = transition.y1 + stop2->offset * (transition.y2 - transition.y1);            
            line.x2 = transition.x2;
            line.y2 = transition.y2;
            TwoStopLinearGradient(d, bbox, &line,
                    stop2->color, stop2->color, stop2->opacity, stop2->opacity);
        }
        
        /*
         * Paint all stops pairwise.
         */
        for (i = 0; i < nstops - 1; i++) {
            stop1 = fillPtr->stops[i];
            stop2 = fillPtr->stops[i+1];
            
            /* If the two offsets identical then skip. */
            if (fabs(stop1->offset - stop2->offset) < 1e-6) {
                continue;
            }
            
            /* Construct the gradient 'line' by scaling the transition
            * using the stop offsets. */
            line.x1 = transition.x1 + stop1->offset * (transition.x2 - transition.x1);
            line.y1 = transition.y1 + stop1->offset * (transition.y2 - transition.y1);
            line.x2 = transition.x1 + stop2->offset * (transition.x2 - transition.x1);
            line.y2 = transition.y1 + stop2->offset * (transition.y2 - transition.y1);            
            TwoStopLinearGradient(d, bbox, &line,
                    stop1->color, stop2->color, stop1->opacity, stop2->opacity);
        }
    } else if (fillPtr->method == kPathGradientMethodRepeat) {
    
        /* @@@ TODO */
    } else if (fillPtr->method == kPathGradientMethodReflect) {
    
        /* @@@ TODO */
    }
}
