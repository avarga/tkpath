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

extern int gUseAntialiasing;

#define MakeGDIPlusColor(xc, opacity) 	Color((int) (opacity*255), 				\
                                                (((xc)->pixel & 0xFF)),			\
                                                (((xc)->pixel >> 8) & 0xFF),	\
                                                (((xc)->pixel >> 16) & 0xFF))
/*
 * This is perhaps a very stupid thing to do.
 * It limits drawing to this single context at a time.
 * Perhaps some window structure should be augmented???
 */

//static HDC gMemHdc = NULL;

void PathExit(ClientData clientData);


/*
 * This class is a wrapper for path drawing using GDI+ 
 * It keeps storage for Graphics and GraphicsPath objects etc.
 */
class PathC {

  public:
    PathC(Drawable d);
    ~PathC(void);
    
	void BeginPath(Drawable d, Tk_PathStyle *style);
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void CurveTo(float x, float y, float x1, float y1, float x2, float y2);
    void CloseFigure(void);
    void Stroke(Tk_PathStyle *style);
    void Fill(Tk_PathStyle *style);
    void StrokeAndFill(Tk_PathStyle *style);
    void GetCurrentPoint(PointF *pt);
    void PaintLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr, int fillRule);
    
  private:  
    HDC mMemHdc;
    PointF mOrigin;
    PointF mCurrentPoint;
    Graphics *mGraphics;
    GraphicsPath *mPath;
    Drawable mD;
    
    static Pen* PathCreatePen(Tk_PathStyle *style);
    static SolidBrush* PathCreateBrush(Tk_PathStyle *style);

	void Junk(void);

    static int sGdiplusStarted;
    static ULONG_PTR sGdiplusToken;
    static GdiplusStartupOutput sGdiplusStartupOutput;
};

static PathC *gPathBuilderPtr = NULL;

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
    return;    
}

PathC::~PathC(void)
{
    delete mPath;
    delete mGraphics;
    DeleteDC(mMemHdc);
}

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

Pen* PathC::PathCreatePen(Tk_PathStyle *style)
{
	LineCap cap;
	DashCap dashCap;
	Pen *penPtr;
    Tk_Dash *dash;
    
    penPtr = new Pen(MakeGDIPlusColor(style->strokeColor, style->strokeOpacity), (float) style->strokeWidth);

	cap = static_cast<LineCap>(TableLookup(LineCapStyleLookupTable, 4, style->capStyle));
    dashCap = static_cast<DashCap>(TableLookup(DashCapStyleLookupTable, 4, style->capStyle));
    penPtr->SetLineCap(cap, cap, dashCap);
    
    penPtr->SetLineJoin(static_cast<LineJoin>(TableLookup(LineJoinStyleLookupTable, 3, style->joinStyle)));
    
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
        // todo
    }
    return penPtr;
}

SolidBrush* PathC::PathCreateBrush(Tk_PathStyle *style)
{
    SolidBrush *brushPtr;
    
    brushPtr = new SolidBrush(MakeGDIPlusColor(style->strokeColor, style->strokeOpacity));
    return brushPtr;
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

void PathC::CurveTo(float x, float y, float x1, float y1, float x2, float y2)
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

void PathC::StrokeAndFill(Tk_PathStyle *style)
{
    Pen *pen = PathCreatePen(style);
    SolidBrush *brush = PathCreateBrush(style);
    mGraphics->FillPath(brush, mPath);
    mGraphics->DrawPath(pen, mPath);
	delete pen;
	delete brush;
}

void PathC::PaintLinearGradient()
{
/*
  LinearGradientBrush brush(
      PointF(0.8f, 1.6f),
      PointF(3.0f, 2.4f),
      Color(255, 255, 0, 0),   // red
      Color(255, 0, 0, 255));  // blue
      */
}

/* 
 * Exit procedure for Tcl. 
 */

void PathExit(ClientData clientData)
{
    //GdiplusShutdown(sGdiplusToken);
}

/*
 * Standard tkpath interface.
 */
 
void TkPathInit(Drawable d)
{
    if (gPathBuilderPtr != NULL) {
        Tcl_Panic("the path drawing context gPathBuilder is already in use\n");
    }
    gPathBuilderPtr = new PathC(d);
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
    // TODO
}

void TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y)
{
    double x31, y31, x32, y32;
    
    // conversion of quadratic bezier curve to cubic bezier curve: (mozilla/svg)
    /* Unchecked! Must be an approximation! */
    x31 = mCurrentPoint.x + (ctrlX - mCurrentPoint.x) * 2 / 3;
    y31 = mCurrentPoint.y + (ctrlY - mCurrentPoint.y) * 2 / 3;
    x32 = ctrlX + (x - ctrlX) / 3;
    y32 = ctrlY + (y - ctrlY) / 3;
    gPathBuilderPtr->CurveTo((float) x31, (float) y31, (float) x32, (float) y32, (float) x, (float) y);
}

void TkPathCurveTo(Drawable d, double ctrlX1, double ctrlY1, 
        double ctrlX2, double ctrlY2, double x, double y)
{
    gPathBuilderPtr->CurveTo((float) ctrlX, (float) ctrlY, (float) ctrlX2, (float) ctrlY2, (float) x, (float) y);
}


void TkPathArcTo(Drawable d,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x2, double y2)
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
    // empty ?
}

void
TkPathFree(Drawable d)
{
    delete gPathBuilderPtr;
    gPathBuilderPtr = NULL;
}

void TkPathClipToPath(Drawable d, int fillRule)
{
    gPathBuilderPtr->
}

void TkPathReleaseClipToPath(Drawable d)
{

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
    ptPtr->x = (double) pf.x;
    ptPtr->y = (double) pf.y;
}

int	TkPathDrawingDestroysPath(void)
{
    return 0;
}

void TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{


    //PointF

}


