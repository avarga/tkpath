/*
 * tkWinGDIPlusPath.c --
 *
 *		This file implements path drawing API's on Windows using the GDI+ lib.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * $Id$
 */

#include <tkWinInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

// this just causes problems here!
#undef Region

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>
#include <Gdiplus.h>

using namespace Gdiplus;

extern "C" int gUseAntiAlias;

#define MakeGDIPlusColor(xc, opacity) 	Color((int) (opacity*255), 				\
                                                (((xc)->pixel & 0xFF)),			\
                                                (((xc)->pixel >> 8) & 0xFF),	\
                                                (((xc)->pixel >> 16) & 0xFF))

static LookupTable LineCapStyleLookupTable[] = {
    {CapNotLast, 	LineCapFlat},
    {CapButt, 	 	LineCapFlat},
    {CapRound, 	 	LineCapRound},
    {CapProjecting, LineCapSquare}
};

static LookupTable DashCapStyleLookupTable[] = {
    {CapNotLast, 	DashCapFlat},
    {CapButt, 	 	DashCapFlat},
    {CapRound, 	 	DashCapRound},
    {CapProjecting, DashCapRound}
};

static LookupTable LineJoinStyleLookupTable[] = {
    {JoinMiter, LineJoinMiter},
    {JoinRound,	LineJoinRound},
    {JoinBevel, LineJoinBevel}
};

/*
 * This is used as a place holder for platform dependent stuff between each call.
 */
typedef struct TkPathContext_ {
    Drawable 		d;
    PathC *		 	c;
} TkPathContext_;

void PathExit(ClientData clientData);


/*
 * This class is a wrapper for path drawing using GDI+ 
 * It keeps storage for Graphics and GraphicsPath objects etc.
 */
class PathC {

  public:
    PathC(Drawable d);
    ~PathC(void);

	static int sGdiplusStarted;
    static ULONG_PTR sGdiplusToken;
    static GdiplusStartupOutput sGdiplusStartupOutput;

    void PushTMatrix(TMatrix *m);
	void BeginPath(Drawable d, Tk_PathStyle *style);
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void CurveTo(float x1, float y1, float x2, float y2, float x, float y);
    void CloseFigure(void);
    void Stroke(Tk_PathStyle *style);
    void Fill(Tk_PathStyle *style);
    void FillAndStroke(Tk_PathStyle *style);
    void GetCurrentPoint(PointF *pt);
    void FillTwoStopLinearGradient(PathRect *bbox, PathRect *line,
            XColor *color1, XColor *color2, double opacity1, double opacity2);
    void FillSimpleLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr);
    
  private:  
    HDC 			mMemHdc;
    PointF 			mOrigin;
    PointF 			mCurrentPoint;
    Graphics 		*mGraphics;
    GraphicsPath 	*mPath;
    Drawable 		mD;
    
    static Pen* PathCreatePen(Tk_PathStyle *style);
    static SolidBrush* PathCreateBrush(Tk_PathStyle *style);
};

/*
 * This is perhaps a very stupid thing to do.
 * It limits drawing to this single context at a time.
 * Perhaps some window structure should be augmented???
 */

//static PathC *gPathBuilderPtr = NULL;

int	PathC::sGdiplusStarted;
ULONG_PTR PathC::sGdiplusToken;
GdiplusStartupOutput PathC::sGdiplusStartupOutput;

PathC::PathC(Drawable d)
{
    TkWinDrawable *twdPtr = (TkWinDrawable *) d;
    //TkWinDrawable *twdPtr = static_cast<TkWinDrawable*>(d);

	if (!sGdiplusStarted) {
        //Status status;
        GdiplusStartupInput gdiplusStartupInput;
        
        GdiplusStartup(&sGdiplusToken, &gdiplusStartupInput, &sGdiplusStartupOutput);
        /*status = GdiplusStartup(&sGdiplusToken, &gdiplusStartupInput, &sGdiplusStartupOutput);
        if (status != Ok) {
            return;
        }*/
        Tcl_CreateExitHandler(PathExit, NULL);
        sGdiplusStarted = 1;
    }
    mD = d;

    /* This will only work for bitmaps; need something else! TkWinGetDrawableDC()? */
    mMemHdc = CreateCompatibleDC(NULL);
    SelectObject(mMemHdc, twdPtr->bitmap.handle);
    mGraphics = new Graphics(mMemHdc);
    if (gUseAntiAlias) {
        mGraphics->SetSmoothingMode(SmoothingModeAntiAlias);
    }
    /* from tile
    TkWinDCState dcState;
    HDC hdc = TkWinGetDrawableDC(Tk_Display(tkwin), d, &dcState);
    ...
    TkWinReleaseDrawableDC(d, hdc, &dcState);
    */
    return;    
}

inline PathC::~PathC(void)
{
    delete mPath;
    delete mGraphics;
    DeleteDC(mMemHdc);
}

Pen* PathC::PathCreatePen(Tk_PathStyle *style)
{
	LineCap 	cap;
	DashCap 	dashCap;
    LineJoin 	lineJoin;
	Pen 		*penPtr;
    Tk_Dash 	*dash;
    
    penPtr = new Pen(MakeGDIPlusColor(style->strokeColor, style->strokeOpacity), (float) style->strokeWidth);

	cap     = static_cast<LineCap>(TableLookup(LineCapStyleLookupTable, 4, style->capStyle));
    dashCap = static_cast<DashCap>(TableLookup(DashCapStyleLookupTable, 4, style->capStyle));
    penPtr->SetLineCap(cap, cap, dashCap);
    
    lineJoin = static_cast<LineJoin>(TableLookup(LineJoinStyleLookupTable, 3, style->joinStyle));
    penPtr->SetLineJoin(lineJoin);
    
    penPtr->SetMiterLimit((float) style->miterLimit);

    dash = &(style->dash);
    if ((dash != NULL) && (dash->number != 0)) {
        int	len;
        float *array;
    
        PathParseDashToArray(dash, style->strokeWidth, &len, &array);
        if (len > 0) {
			penPtr->SetDashPattern(array, len);
			ckfree((char *) array);
        }
        penPtr->SetDashOffset((float) style->offset);
    }    
    if (style->strokeStipple != None) {
        /* @@@ TODO */
    }
    return penPtr;
}

inline SolidBrush* PathC::PathCreateBrush(Tk_PathStyle *style)
{
    SolidBrush 	*brushPtr;
    
    brushPtr = new SolidBrush(MakeGDIPlusColor(style->fillColor, style->fillOpacity));
    return brushPtr;
}

inline void PathC::PushTMatrix(TMatrix *tm)
{
    Matrix m((float)tm->a, (float)tm->b, (float)tm->c, (float)tm->d, (float)tm->tx, (float)tm->ty);
    mGraphics->MultiplyTransform(&m);
}

inline void PathC::BeginPath(Drawable d, Tk_PathStyle *style)
{
    mPath = new GraphicsPath((style->fillRule == WindingRule) ? FillModeWinding : FillModeAlternate);
}

inline void PathC::MoveTo(float x, float y)
{
    mPath->StartFigure();
    mOrigin.X = (float) x;
    mOrigin.Y = (float) y;
    mCurrentPoint.X = (float) x;
    mCurrentPoint.Y = (float) y;
}

inline void PathC::LineTo(float x, float y)
{
    mPath->AddLine(mCurrentPoint.X, mCurrentPoint.Y, x, y);
    mCurrentPoint.X = x;
    mCurrentPoint.Y = y;
}

inline void PathC::CurveTo(float x1, float y1, float x2, float y2, float x, float y)
{
    mPath->AddBezier(mCurrentPoint.X, mCurrentPoint.Y, // startpoint
            x1, y1, x2, y2, // controlpoints
            x, y); 			// endpoint
    mCurrentPoint.X = x;
    mCurrentPoint.Y = y;
}

inline void PathC::CloseFigure()
{
    mPath->CloseFigure();
    mCurrentPoint.X = mOrigin.X;
    mCurrentPoint.Y = mOrigin.Y;
}

inline void PathC::Stroke(Tk_PathStyle *style)
{
    Pen *pen = PathCreatePen(style);
    
    mGraphics->DrawPath(pen, mPath);
	delete pen;
}

inline void PathC::Fill(Tk_PathStyle *style)
{
    SolidBrush *brush = PathCreateBrush(style);
    
    mGraphics->FillPath(brush, mPath);
	delete brush;
}

inline void PathC::FillAndStroke(Tk_PathStyle *style)
{
    Pen 		*pen = PathCreatePen(style);
    SolidBrush 	*brush = PathCreateBrush(style);
    
    mGraphics->FillPath(brush, mPath);
    mGraphics->DrawPath(pen, mPath);
	delete pen;
	delete brush;
}

inline void PathC::GetCurrentPoint(PointF *pt)
{
    *pt = mCurrentPoint;
}

/* ONLY for nstops = 2 */

void PathC::FillSimpleLinearGradient(
        PathRect *bbox, 		/* The items bounding box in untransformed coords. */
        LinearGradientFill *fillPtr)
{
    PathRect 		transition;
    GradientStop 	*stop1, *stop2;
    PointF			p1, p2;
    Color			col1, col2;

    transition = fillPtr->transition;
    stop1 = fillPtr->stops[0];
    stop2 = fillPtr->stops[1];

    p1.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*transition.x1);
    p1.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*transition.y1);
    p2.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*transition.x2);
    p2.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*transition.y2);

    col1 = MakeGDIPlusColor(stop1->color, stop1->opacity);
    col2 = MakeGDIPlusColor(stop2->color, stop2->opacity);
    LinearGradientBrush brush(p1, p2, col1, col2);
    if ((stop1->offset > 1e-6) || (stop2->offset < 1.0-1e-6)) {
    
        /* This is a trick available in gdi+ we use for padding with a const color. */
        REAL blendFactors[4];
        REAL blendPositions[4];
        blendFactors[0] = 0.0;
        blendFactors[1] = 0.0;
        blendFactors[2] = 1.0;
        blendFactors[3] = 1.0;
        blendPositions[0] = 0.0;
        blendPositions[1] = (float) stop1->offset;
        blendPositions[2] = (float) stop2->offset;
        blendPositions[3] = 1.0;
        brush.SetBlend(blendFactors, blendPositions, 4);
    }
    /* We could also have used brush.SetWrapMode() */
    mGraphics->FillPath(&brush, mPath);
}

void PathC::FillTwoStopLinearGradient(
        PathRect *bbox, /* The items bounding box in untransformed coords. */
        PathRect *line,	/* The relative line that defines the
                         * gradient transition line. 
                         * We paint perp to this line. */
        XColor *color1, XColor *color2, double opacity1, double opacity2)
{
    double			p1x, p1y, p2x, p2y;
    double			ax, ay, a2, alen;
    double			q1x, q1y, q2x, q2y;
	double			diag;
    PointF			p1, p2;
    PointF			q1, q2, a;
    Color			col1, col2;
      
    /*
     * Scale up 'line' vector to bbox.
     * Vectors: 
     *		p1 = start transition vector
     *		p2 = end transition vector
     *		a  = P2 - P1
     */
    p1x = bbox->x1 + (bbox->x2 - bbox->x1)*line->x1;
    p1y = bbox->y1 + (bbox->y2 - bbox->y1)*line->y1;
    p2x = bbox->x1 + (bbox->x2 - bbox->x1)*line->x2;
    p2y = bbox->y1 + (bbox->y2 - bbox->y1)*line->y2;
    ax = p2x - p1x;
    ay = p2y - p1y;
    a2 = ax*ax + ay*ay;
    if (a2 < 0.5) {
        /* Not much to paint in this case. */
        return;
    }
    alen = sqrt(a2);
#if 0
    p1.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*line->x1);
    p1.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*line->y1);
    p2.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*line->x2);
    p2.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*line->y2);
    a.X = p2.X - p1.X;
    a.Y = p2.Y - p1.Y;
    a2 = a.X*a.X + a.Y*a.Y;
    if (a2 < 0.5) {
        /* Not much to paint in this case. */
        return;
    }
    alen = sqrt(a2);
#endif    
    /* GDI+ has a very awkward way of repeating the gradient pattern
     * for the complete fill region that can't be switched off.
     * Need therefore to clip away any region outside the wanted one.
     */

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
     
    /* The idea here is to intersect the region formed by q1, q2, q1+a, q2+a,
     * with the path region. 
     */
    /* Smarter? path.AddPolygon(polyPoints, 4); */
    GraphicsPath 	clipPath;
    clipPath.StartFigure();
    clipPath.AddLine((float) q1x, (float) q1y, (float) q2x, (float) q2y);
    clipPath.AddLine((float) q2x, (float) q2y, (float) (q2x+ax), (float) (q2y+ay));
    clipPath.AddLine((float) (q2x+ax), (float) (q2y+ay), (float) (q1x+ax), (float) (q1y+ay));
    clipPath.CloseFigure();
    
    GraphicsContainer container = mGraphics->BeginContainer();

    Region region(&clipPath);
    mGraphics->SetClip(&region);

    p1 = PointF((float) p1x, (float) p1y);
    p2 = PointF((float) p2x, (float) p2y);
    col1 = MakeGDIPlusColor(color1, opacity1);
    col2 = MakeGDIPlusColor(color2, opacity2);
    LinearGradientBrush brush(p1, p2, col1, col2);
    mGraphics->FillPath(&brush, mPath);

    mGraphics->EndContainer(container);
}

/*
    LinearGradientBrush linGrBrush(
        Point(0, 0),
        Point(200, 100),
        Color(255, 0, 0, 255),   // opaque blue
        Color(255, 0, 255, 0));  // opaque green
    Pen pen(&linGrBrush, 10);
    
    graphics.DrawLine(&pen, 0, 0, 600, 300);
    graphics.FillEllipse(&linGrBrush, 10, 100, 200, 100);
*/

/* 
 * Exit procedure for Tcl. 
 */

void PathExit(ClientData clientData)
{
    if (PathC::sGdiplusStarted) {
        GdiplusShutdown(PathC::sGdiplusToken);
    }
}

/*
 * Standard tkpath interface.
 * More or less a wrapper for the class PathC.
 * Is there a smarter way?
 */
 
TkPathContext TkPathInit(Tk_Window tkwin, Drawable d)
{
    TkPathContext_ *context = (TkPathContext_ *) ckalloc((unsigned) (sizeof(TkPathContext_)));
    context->d = d;
    context->c = new PathC(d);
    return (TkPathContext) context;
}

void TkPathPushTMatrix(TkPathContext ctx, TMatrix *m)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->PushTMatrix(m);
}

void TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->BeginPath(d, style);
}

void TkPathMoveTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->MoveTo((float) x, (float) y);
}

void TkPathLineTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->LineTo((float) x, (float) y);
}

void TkPathLinesTo(TkPathContext ctx, double *pts, int n)
{
    /* @@@ TODO */
}

void TkPathQuadBezier(TkPathContext ctx, double ctrlX, double ctrlY, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    double x31, y31, x32, y32;
    PointF cp;
    
    context->c->GetCurrentPoint(&cp);
    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = cp.X + (ctrlX - cp.X) * 2 / 3;
    y31 = cp.Y + (ctrlY - cp.Y) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;
    context->c->CurveTo((float) x31, (float) y31, (float) x32, (float) y32, (float) x, (float) y);
}

void TkPathCurveTo(TkPathContext ctx, double ctrlX1, double ctrlY1, 
        double ctrlX2, double ctrlY2, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->CurveTo((float) ctrlX1, (float) ctrlY1, (float) ctrlX2, (float) ctrlY2, (float) x, (float) y);
}


void TkPathArcTo(TkPathContext ctx,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    TkPathArcToUsingBezier(d, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void
TkPathRect(TkPathContext ctx, double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->
}

void
TkPathOval(TkPathContext ctx, double cx, double cy, double rx, double ry)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->
}

void
TkPathClosePath(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->CloseFigure();
}

void
TkPathEndPath(TkPathContext ctx)
{
    // @@@ empty ?
}

void
TkPathFree(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    delete context->c;
    ckfree((char *) context);
}

void TkPathClipToPath(TkPathContext ctx, int fillRule)
{
    /* empty */
}

void TkPathReleaseClipToPath(TkPathContext ctx)
{
    /* empty */
}

void TkPathStroke(TkPathContext ctx, Tk_PathStyle *style)
{       
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->Stroke(style);
}

void TkPathFill(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->Fill(style);
}

void TkPathFillAndStroke(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->FillAndStroke(style);
}

int TkPathGetCurrentPosition(TkPathContext ctx, PathPoint *ptPtr)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    PointF pf;
    context->c->GetCurrentPoint(&pf);
    ptPtr->x = (double) pf.X;
    ptPtr->y = (double) pf.Y;
    return TCL_OK;
}

int	TkPathDrawingDestroysPath(void)
{
    return 0;
}

/* @@@ INCOMPLETE! We need to consider any padding as well. */

void TkPathPaintLinearGradient(TkPathContext ctx, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    int 			i;
    int 			nstops;
    PathRect 		line, transition;
    GradientStop 	*stop1, *stop2;

    transition = fillPtr->transition;
    nstops = fillPtr->nstops;
    
    if (nstops == 1) {
        /* Fill using solid color. */
    } else if (nstops == 2) {
    
        /* Use a simplified version in this case. */
        context->c->FillSimpleLinearGradient(bbox, fillPtr);
    } else {
    
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
            * using the stop offsets. 
            */
            line.x1 = transition.x1 + stop1->offset * (transition.x2 - transition.x1);
            line.y1 = transition.y1 + stop1->offset * (transition.y2 - transition.y1);
            line.x2 = transition.x1 + stop2->offset * (transition.x2 - transition.x1);
            line.y2 = transition.y1 + stop2->offset * (transition.y2 - transition.y1);            
            context->c->FillTwoStopLinearGradient(bbox, &line,
                    stop1->color, stop2->color, stop1->opacity, stop2->opacity);
        }
    }
}



