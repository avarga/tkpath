/*
 * tkWinGDIPlusPath.c --
 *
 *		This file implements path drawing API's on Windows using the GDI+ lib.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
#include <tkWinInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

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

void PathExit(ClientData clientData);


/*
 * This class is a wrapper for path drawing using GDI+ 
 * It keeps storage for Graphics and GraphicsPath objects etc.
 */
class PathC {

  public:
    PathC(Drawable d);
    ~PathC(void);
    
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
    void PaintLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr, int fillRule);
    
  private:  
    HDC 			mMemHdc;
    PointF 			mOrigin;
    PointF 			mCurrentPoint;
    Graphics 		*mGraphics;
    GraphicsPath 	*mPath;
    Drawable 		mD;
    
    static Pen* PathCreatePen(Tk_PathStyle *style);
    static SolidBrush* PathCreateBrush(Tk_PathStyle *style);

	static int sGdiplusStarted;
    static ULONG_PTR sGdiplusToken;
    static GdiplusStartupOutput sGdiplusStartupOutput;
};

/*
 * This is perhaps a very stupid thing to do.
 * It limits drawing to this single context at a time.
 * Perhaps some window structure should be augmented???
 */

static PathC *gPathBuilderPtr = NULL;

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
    return;    
}

PathC::~PathC(void)
{
    delete mPath;
    delete mGraphics;
    DeleteDC(mMemHdc);
}

Pen* PathC::PathCreatePen(Tk_PathStyle *style)
{
	LineCap cap;
	DashCap dashCap;
    LineJoin lineJoin;
	Pen *penPtr;
    Tk_Dash *dash;
    
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

SolidBrush* PathC::PathCreateBrush(Tk_PathStyle *style)
{
    SolidBrush *brushPtr;
    
    brushPtr = new SolidBrush(MakeGDIPlusColor(style->fillColor, style->fillOpacity));
    return brushPtr;
}

void PathC::PushTMatrix(TMatrix *tm)
{
    Matrix m((float)tm->a, (float)tm->b, (float)tm->c, (float)tm->d, (float)tm->tx, (float)tm->ty);
    mGraphics->MultiplyTransform(&m);
}

void PathC::BeginPath(Drawable d, Tk_PathStyle *style)
{
    mPath = new GraphicsPath((style->fillRule == WindingRule) ? FillModeWinding : FillModeAlternate);
}

void PathC::MoveTo(float x, float y)
{
    mPath->StartFigure();
    mOrigin.X = (float) x;
    mOrigin.Y = (float) y;
    mCurrentPoint.X = (float) x;
    mCurrentPoint.Y = (float) y;
}

void PathC::LineTo(float x, float y)
{
    mPath->AddLine(mCurrentPoint.X, mCurrentPoint.Y, x, y);
    mCurrentPoint.X = x;
    mCurrentPoint.Y = y;
}

void PathC::CurveTo(float x1, float y1, float x2, float y2, float x, float y)
{
    mPath->AddBezier(mCurrentPoint.X, mCurrentPoint.Y, // startpoint
            x1, y1, x2, y2, // controlpoints
            x, y); // endpoint
    mCurrentPoint.X = x;
    mCurrentPoint.Y = y;
}

void PathC::CloseFigure()
{
    mPath->CloseFigure();
    mCurrentPoint.X = mOrigin.X;
    mCurrentPoint.Y = mOrigin.Y;
}

void PathC::Stroke(Tk_PathStyle *style)
{
    Pen *pen = PathCreatePen(style);
    
    mGraphics->DrawPath(pen, mPath);
	delete pen;
}

void PathC::Fill(Tk_PathStyle *style)
{
    SolidBrush *brush = PathCreateBrush(style);
    
    mGraphics->FillPath(brush, mPath);
	delete brush;
}

void PathC::FillAndStroke(Tk_PathStyle *style)
{
    Pen *pen = PathCreatePen(style);
    SolidBrush *brush = PathCreateBrush(style);
    
    mGraphics->FillPath(brush, mPath);
    mGraphics->DrawPath(pen, mPath);
	delete pen;
	delete brush;
}

void PathC::GetCurrentPoint(PointF *pt)
{
    *pt = mCurrentPoint;
}

void PathC::PaintLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
/*
  LinearGradientBrush brush(
      PointF(0.8f, 1.6f),
      PointF(3.0f, 2.4f),
      Color(255, 255, 0, 0),   // red
      Color(255, 0, 0, 255));  // blue
      
    LinearGradientBrush linGrBrush(
        Point(0, 0),
        Point(200, 100),
        Color(255, 0, 0, 255),   // opaque blue
        Color(255, 0, 255, 0));  // opaque green

    Pen pen(&linGrBrush, 10);
    
    graphics.DrawLine(&pen, 0, 0, 600, 300);
    graphics.FillEllipse(&linGrBrush, 10, 100, 200, 100);
      */
}

/* 
 * Exit procedure for Tcl. 
 */

void PathExit(ClientData clientData)
{
    GdiplusShutdown(sGdiplusToken);
}

/*
 * Standard tkpath interface.
 * More or less a wrapper for the class PathC.
 * Is there a smarter way?
 */
 
void TkPathInit(Drawable d)
{
    if (gPathBuilderPtr != NULL) {
        Tcl_Panic("the path drawing context gPathBuilder is already in use\n");
    }
    gPathBuilderPtr = new PathC(d);
}

void
TkPathPushTMatrix(Drawable d, TMatrix *m)
{
    gPathBuilderPtr->PushTMatrix(m);
}

void TkPathBeginPath(Drawable d, Tk_PathStyle *style)
{
    gPathBuilderPtr->BeginPath(d, style);
}

void TkPathMoveTo(Drawable d, double x, double y)
{
    gPathBuilderPtr->MoveTo((float) x, (float) y);
}

void TkPathLineTo(Drawable d, double x, double y)
{
    gPathBuilderPtr->LineTo((float) x, (float) y);
}

void TkPathLinesTo(Drawable d, double *pts, int n)
{
    /* @@@ TODO */
}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double x31, y31, x32, y32;
    PointF cp;
    
    gPathBuilderPtr->GetCurrentPoint(&cp);
    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = cp.X + (ctrlX - cp.X) * 2 / 3;
    y31 = cp.Y + (ctrlY - cp.Y) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;
    gPathBuilderPtr->CurveTo((float) x31, (float) y31, (float) x32, (float) y32, (float) x, (float) y);
}

void TkPathCurveTo(Drawable d, double ctrlX1, double ctrlY1, 
        double ctrlX2, double ctrlY2, double x, double y)
{
    gPathBuilderPtr->CurveTo((float) ctrlX1, (float) ctrlY1, (float) ctrlX2, (float) ctrlY2, (float) x, (float) y);
}


void TkPathArcTo(Drawable d,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x, double y)
{
    TkPathArcToUsingBezier(d, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void
TkPathClosePath(Drawable d)
{
    gPathBuilderPtr->CloseFigure();
}

void
TkPathEndPath(Drawable d)
{
    // @@@ empty ?
}

void
TkPathFree(Drawable d)
{
    delete gPathBuilderPtr;
    gPathBuilderPtr = NULL;
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
    gPathBuilderPtr->Stroke(style);
}

void TkPathFill(Drawable d, Tk_PathStyle *style)
{
    gPathBuilderPtr->Fill(style);
}

void TkPathFillAndStroke(Drawable d, Tk_PathStyle *style)
{
    gPathBuilderPtr->FillAndStroke(style);
}

int TkPathGetCurrentPosition(Drawable d, PathPoint *ptPtr)
{
    PointF pf;
    gPathBuilderPtr->GetCurrentPoint(&pf);
    ptPtr->x = (double) pf.X;
    ptPtr->y = (double) pf.Y;
    return TCL_OK;
}

int	TkPathDrawingDestroysPath(void)
{
    return 0;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{


    //PointF

}


