/*
 * tkWinGDIPlusPath.c --
 *
 *		This file implements path drawing API's on Windows using the GDI+ lib.
 *
 * Copyright (c) 2005-2007  Mats Bengtsson
 *
 * $Id$
 */

#include <tkWinInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>
#include <Gdiplus.h>

using namespace Gdiplus;

extern Tcl_Interp *gInterp;
extern "C" int gUseAntiAlias;
extern "C" int gSurfaceCopyPremultiplyAlpha;

#define MakeGDIPlusColor(xc, opacity) 	Color(BYTE(opacity*255), 				\
											BYTE(((xc)->pixel & 0xFF)),			\
											BYTE(((xc)->pixel >> 8) & 0xFF),	\
											BYTE(((xc)->pixel >> 16) & 0xFF))

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

static int sGdiplusStarted;
static ULONG_PTR sGdiplusToken;
static GdiplusStartupOutput sGdiplusStartupOutput;

static void InitGDIplus(void);

static void PathExit(ClientData clientData);

/*
 * This class is a wrapper for path drawing using GDI+ 
 * It keeps storage for Graphics and GraphicsPath objects etc.
 */
class PathC {

  public:
    PathC(HDC hdc);
    ~PathC(void);

    void PushTMatrix(TMatrix *m);
    void SaveState();
    void RestoreState();
	void BeginPath(Tk_PathStyle *style);
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void CurveTo(float x1, float y1, float x2, float y2, float x, float y);
	void AddRectangle(float x, float y, float width, float height);
	void AddEllipse(float cx, float cy, float rx, float ry);
    void DrawImage(Tk_PhotoHandle photo, float x, float y, float width, float height);
    void DrawString(Tk_PathStyle *style, Tk_PathTextStyle *textStylePtr, 
        float x, float y, char *utf8);
    void CloseFigure(void);
    void Stroke(Tk_PathStyle *style);
    void Fill(Tk_PathStyle *style);
    void FillAndStroke(Tk_PathStyle *style);
    void GetCurrentPoint(PointF *pt);
	void FillLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr, int fillRule);
    void FillRadialGradient(PathRect *bbox, RadialGradientFill *fillPtr, int fillRule);

  private:  
    HDC 				mMemHdc;
    PointF 				mOrigin;
    PointF 				mCurrentPoint;
    Graphics 			*mGraphics;
    GraphicsPath 		*mPath;
    GraphicsContainer 	mContainerStack[10];
    int					mCointainerTop;
    
    static Pen* PathCreatePen(Tk_PathStyle *style);
    static SolidBrush* PathCreateBrush(Tk_PathStyle *style);
};

typedef struct PathSurfaceGDIpRecord {
    HBITMAP bitmap;
    void * 	data;
    int 	width;
    int		height;
    int		stride;		/* the number of bytes between the start of rows in the buffer */
} PathSurfaceGDIpRecord;

/*
 * This is used as a place holder for platform dependent stuff between each call.
 */
typedef struct TkPathContext_ {
	PathC *		c;
    HDC			memHdc;
    PathSurfaceGDIpRecord *	surface;	/* NULL unless surface. */
};

void InitGDIplus(void)
{
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

PathC::PathC(HDC hdc)
{
	if (!sGdiplusStarted) {
		InitGDIplus();
    }
    mMemHdc = hdc;
    mGraphics = new Graphics(mMemHdc);
	mPath = NULL;
	mCointainerTop = 0;
    if (gUseAntiAlias) {
        mGraphics->SetSmoothingMode(SmoothingModeAntiAlias);
    }
    return;    
}

inline PathC::~PathC(void)
{
	if (mPath) {
		delete mPath;
	}
    if (mGraphics) {
        delete mGraphics;
    }
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

inline void PathC::SaveState()
{
    if (mCointainerTop >= 9) {
        Tcl_Panic("reached top of cointainer stack of GDI+");
    }
    mContainerStack[mCointainerTop] = mGraphics->BeginContainer();
    mCointainerTop++;
}

inline void PathC::RestoreState()
{
    mCointainerTop--;
    mGraphics->EndContainer(mContainerStack[mCointainerTop]);
}

inline void PathC::BeginPath(Tk_PathStyle *style)
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

inline void PathC::AddRectangle(float x, float y, float width, float height)
{
    RectF rect(x, y, width, height);
    mPath->AddRectangle(rect);
    // @@@ this depends
    mCurrentPoint.X = x;
    mCurrentPoint.Y = y;
}

inline void PathC::AddEllipse(float cx, float cy, float rx, float ry)
{
    mPath->AddEllipse(cx-rx, cy-ry, 2*rx, 2*ry);
    // @@@ this depends
    mCurrentPoint.X = cx+rx;
    mCurrentPoint.Y = cy;
}

inline void PathC::DrawImage(Tk_PhotoHandle photo, float x, float y, float width, float height)
{
    Tk_PhotoImageBlock block;
    PixelFormat format;
    INT stride;
    int iwidth, iheight;
	int pitch;
    int smallEndian = 1;	/* Hardcoded. */
    unsigned char *data = NULL;
    unsigned char *ptr = NULL;
    unsigned char *srcPtr, *dstPtr;
    int srcR, srcG, srcB, srcA;		/* The source pixel offsets. */
    int dstR, dstG, dstB, dstA;		/* The destination pixel offsets. */
    int i, j;
    
    Tk_PhotoGetImage(photo, &block);
    iwidth = block.width;
    iheight = block.height;
    stride = block.pitch;
    pitch = block.pitch;
    if (width == 0.0) {
        width = (float) iwidth;
    }
    if (height == 0.0) {
        height = (float) iheight;
    }
    
    if (block.pixelSize*8 == 32) {
        format = PixelFormat32bppARGB;

        srcR = block.offset[0];
        srcG = block.offset[1]; 
        srcB = block.offset[2];
        srcA = block.offset[3];
        dstR = 1;
        dstG = 2;
        dstB = 3;
        dstA = 0;
        if (smallEndian) {
            dstR = 3-dstR, dstG = 3-dstG, dstB = 3-dstB, dstA = 3-dstA;
        }
        if ((srcR == dstR) && (srcG == dstG) && (srcB == dstB) && (srcA == dstA)) {
            ptr = (unsigned char *) block.pixelPtr;
        } else {
            data = (unsigned char *) ckalloc(pitch*iheight);
            ptr = data;
            
            for (i = 0; i < iheight; i++) {
                srcPtr = block.pixelPtr + i*pitch;
                dstPtr = ptr + i*pitch;
                for (j = 0; j < iwidth; j++) {
                    *(dstPtr+dstR) = *(srcPtr+srcR);
                    *(dstPtr+dstG) = *(srcPtr+srcG);
                    *(dstPtr+dstB) = *(srcPtr+srcB);
                    *(dstPtr+dstA) = *(srcPtr+srcA);
                    srcPtr += 4;
                    dstPtr += 4;
                }
            }
        }
    } else if (block.pixelSize*8 == 24) {
        /* Could do something about this? */
        return;
    } else {
        return;
    }
    Bitmap bitmap(iwidth, iheight, stride, format, (BYTE *)ptr);
    mGraphics->DrawImage(&bitmap, x, y, width, height);
    if (data) {
        ckfree((char *)data);
    }
}

inline void PathC::DrawString(Tk_PathStyle *style, Tk_PathTextStyle *textStylePtr, 
        float x, float y, char *utf8)
{
    Tcl_DString ds, dsFont;
    Tcl_UniChar *uniPtr;
    
	Tcl_DStringInit(&dsFont);
    FontFamily fontFamily((const WCHAR *)Tcl_UtfToUniCharDString(textStylePtr->fontFamily, -1, &dsFont));
	if (fontFamily.GetLastStatus() != Ok) {
		fontFamily.GenericSansSerif();
	}
	Gdiplus::Font font(&fontFamily, (float) textStylePtr->fontSize, FontStyleRegular, UnitPixel);
	if (font.GetLastStatus() != Ok) {
		// TODO
	}
	Tcl_DStringFree(&dsFont);
	Tcl_DStringInit(&ds);
	uniPtr = Tcl_UtfToUniCharDString(utf8, -1, &ds);

    /* The fourth argument is a PointF object that contains the 
     * coordinates of the upper-left corner of the string.
     * See GDI+ docs and the FontFamily for translating between
     * design units and pixels.
     */
    float ascentPixels = font.GetSize() * 
            fontFamily.GetCellAscent(FontStyleRegular) / fontFamily.GetEmHeight(FontStyleRegular);
    PointF point(x, y - ascentPixels);
	if (gUseAntiAlias) {
		mGraphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
	}
    if (style->fillColor != NULL) {
        SolidBrush *brush = PathCreateBrush(style);
        mGraphics->DrawString((const WCHAR *)uniPtr, Tcl_UniCharLen(uniPtr), &font, point, brush);
        delete brush;
    }
    if (style->strokeColor != NULL) {
        Pen	*pen = PathCreatePen(style);
        mPath->AddString((const WCHAR *)uniPtr, Tcl_UniCharLen(uniPtr), 
                &fontFamily, FontStyleRegular, (float) textStylePtr->fontSize, point, NULL);
        mGraphics->DrawPath(pen, mPath);
        delete pen;
    }
	Tcl_DStringFree(&ds);
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

#if 0
void PathC::FillSimpleLinearGradient(
        PathRect *bbox, 		/* The items bounding box in untransformed coords. */
        LinearGradientFill *fillPtr)
{
    PathRect 		*tPtr;
    GradientStop 	*stop1, *stop2;
    PointF			p1, p2;
    Color			col1, col2;
	GradientStopArray 	*stopArrPtr;
    
    stopArrPtr = fillPtr->stopArrPtr;

    tPtr = fillPtr->transitionPtr;
    stop1 = stopArrPtr->stops[0];
    stop2 = stopArrPtr->stops[1];

    p1.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*tPtr->x1);
    p1.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*tPtr->y1);
    p2.X = (float) (bbox->x1 + (bbox->x2 - bbox->x1)*tPtr->x2);
    p2.Y = (float) (bbox->y1 + (bbox->y2 - bbox->y1)*tPtr->y2);

    col1 = MakeGDIPlusColor(stop1->color, stop1->opacity);
    col2 = MakeGDIPlusColor(stop2->color, stop2->opacity);
    LinearGradientBrush brush(p1, p2, col1, col2);
    if ((stop1->offset > 1e-6) || (stop2->offset < 1.0-1e-6)) {
    
        /* This is a trick available in gdi+ we use for padding with a const color. ??? */
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
#endif

void PathC::FillLinearGradient(PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    int					i;
    int					nstops;
	float				x, y, width, height;
    GradientStop 		*stop;
    GradientStopArray 	*stopArrPtr;
    PathRect			*tPtr;
	PointF				p1, p2;

    stopArrPtr = fillPtr->stopArrPtr;
    nstops = stopArrPtr->nstops;
    tPtr = fillPtr->transitionPtr;

    GraphicsContainer container = mGraphics->BeginContainer();

    /*
     * We need to do like this since this is how SVG defines gradient drawing
     * in case the transition vector is in relative coordinates.
     */
    if (fillPtr->units == kPathGradientUnitsBoundingBox) {
        x = float(bbox->x1);
        y = float(bbox->y1);
        width = float(bbox->x2 - bbox->x1);
        height = float(bbox->y2 - bbox->y1);
		p1.X = float(x + tPtr->x1*width);
		p1.Y = float(y + tPtr->y1*height);
        p2.X = float(x + tPtr->x2*width);
		p2.Y = float(y + tPtr->y2*height);
    } else {
        p1.X = float(tPtr->x1);
		p1.Y = float(tPtr->y1);
        p2.X = float(tPtr->x2);
		p2.Y = float(tPtr->y2);
    }
    stop = stopArrPtr->stops[0];
    Color col1(MakeGDIPlusColor(stop->color, stop->opacity));
    stop = stopArrPtr->stops[nstops-1];
    Color col2(MakeGDIPlusColor(stop->color, stop->opacity));
	LinearGradientBrush brush(p1, p2, col1, col2);
    if (fillPtr->method == kPathGradientMethodReflect) {
        brush.SetWrapMode(WrapModeTileFlipXY);
    } else if (fillPtr->method == kPathGradientMethodPad) {
        brush.SetWrapMode(WrapModeTileFlipXY);
    }
    Color *col = new Color[nstops];
    REAL *pos = new REAL[nstops];
	for (i = 0; i < nstops; i++) {
        stop = stopArrPtr->stops[i];
        col[i] = MakeGDIPlusColor(stop->color, stop->opacity);
        pos[i] = REAL(stop->offset);
    }
    brush.SetInterpolationColors(col, pos, nstops);
    mGraphics->FillPath(&brush, mPath);
    
    /* 
     * GDI+ seems to miss a simple way to pad with constant colors.
     */
    if (fillPtr->method == kPathGradientMethodPad) {
        PointF pdiff = p2 - p1;
        PointF perp(-pdiff.Y, pdiff.X);
		Gdiplus::Region region(mPath);
        mGraphics->SetClip(&region);
        SolidBrush brush1(col1);
        SolidBrush brush2(col2);
        width = float(bbox->x2 - bbox->x1);
        height = float(bbox->y2 - bbox->y1);
        float side = width + height;
        float angle = atan2(perp.Y, perp.X) * 180.0/M_PI;
        // Paint only if needed!?
        // @@@ I anticipate problems here if any color has opacity unequal to 1!
        mGraphics->FillPie(&brush1, p1.X-side, p1.Y-side, side, side, angle, 180.0);
        mGraphics->FillPie(&brush2, p2.X-side, p2.Y-side, side, side, angle+180.0, 180.0);
    }
    mGraphics->EndContainer(container);
	delete [] col;
	delete [] pos;
}

void PathC::FillRadialGradient(
        PathRect *bbox, 	/* The items bounding box in untransformed coords. */
        RadialGradientFill *fillPtr, int fillRule)
{
    int					i;
    int					nstops;
	float				radius;
	float				x, y, width, height, cx, cy;
    GradientStop 		*stop;
    GradientStopArray 	*stopArrPtr;
    RadialTransition    *tPtr;

    stopArrPtr = fillPtr->stopArrPtr;    
    nstops = stopArrPtr->nstops;
    tPtr = fillPtr->radialPtr;
	radius = float(tPtr->radius);
	x = float(bbox->x1);
	y = float(bbox->y1);
	width = float(bbox->x2 - bbox->x1);
	height = float(bbox->y2 - bbox->y1);
	cx = x + width/2.0;
	cy = y + height/2.0;

    GraphicsContainer container = mGraphics->BeginContainer();
    mGraphics->SetClip(mPath);
	
    stop = stopArrPtr->stops[nstops-1];
    SolidBrush solidBrush(MakeGDIPlusColor(stop->color, stop->opacity));
	mGraphics->FillPath(&solidBrush, mPath);

    GraphicsPath path;
    path.AddEllipse(cx-radius*width, cy-radius*height, 2*radius*width, 2*radius*height);
    PathGradientBrush brush(&path);
    stop = stopArrPtr->stops[0];
    brush.SetCenterColor(MakeGDIPlusColor(stop->color, stop->opacity));
	PointF focal(float(x + tPtr->focalX*width), float(y + tPtr->focalY*height));
    brush.SetCenterPoint(focal);
    int count = 1;
    stop = stopArrPtr->stops[nstops-1];
	Color color = MakeGDIPlusColor(stop->color, stop->opacity);
    brush.SetSurroundColors(&color, &count);
    
	/* gdi+ counts them from the border and not from the center. */
    Color *col = new Color[nstops];
    REAL *pos = new REAL[nstops];
    for (i = nstops-1; i >= 0; i--) {
        stop = stopArrPtr->stops[i];
        col[i] = MakeGDIPlusColor(stop->color, stop->opacity);
        pos[i] = REAL(1.0 - stop->offset);
    }
    brush.SetInterpolationColors(col, pos, nstops);
    mGraphics->FillPath(&brush, &path);
    mGraphics->EndContainer(container);
	delete [] col;
	delete [] pos;
}

/* 
 * Exit procedure for Tcl. 
 */

void PathExit(ClientData clientData)
{
    if (sGdiplusStarted) {
        GdiplusShutdown(sGdiplusToken);
    }
}

/*
 * Standard tkpath interface.
 * More or less a wrapper for the class PathC.
 * Is there a smarter way?
 */
 
TkPathContext TkPathInit(Tk_Window tkwin, Drawable d)
{
    TkPathContext_ *context = reinterpret_cast<TkPathContext_ *> (ckalloc((unsigned) (sizeof(TkPathContext_))));
    TkWinDrawable *twdPtr = (TkWinDrawable *) d;
    HDC memHdc;
    //TkWinDrawable *twdPtr = reinterpret_cast<TkWinDrawable*>(d);
    /* from tile
    TkWinDCState dcState;
    HDC hdc = TkWinGetDrawableDC(Tk_Display(tkwin), d, &dcState);
    ...
    TkWinReleaseDrawableDC(d, hdc, &dcState);
    */

    /* This will only work for bitmaps; need something else! TkWinGetDrawableDC()? */
    memHdc = CreateCompatibleDC(NULL);
    SelectObject(memHdc, twdPtr->bitmap.handle);
    context->c = new PathC(memHdc);
    context->memHdc = memHdc;
    context->surface = NULL;
    return (TkPathContext) context;
}

TkPathContext TkPathInitSurface(int width, int height)
{
    TkPathContext_ *context = reinterpret_cast<TkPathContext_ *> (ckalloc((unsigned) (sizeof(TkPathContext_))));
    PathSurfaceGDIpRecord *surface = (PathSurfaceGDIpRecord *) ckalloc((unsigned) (sizeof(PathSurfaceGDIpRecord)));
    HBITMAP hbm = NULL;
    HDC memHdc = NULL;
    BITMAPINFO *bmInfo = NULL;
    void *data;
    
    memHdc = CreateCompatibleDC(NULL);
#if 0
    hbm = CreateBitmap(width, height, 1, 32, NULL);
#endif
    /* We create off-screen surfaces as DIBs */
    bmInfo = (BITMAPINFO *) ckalloc(sizeof(BITMAPINFO));
    bmInfo->bmiHeader.biSize               = sizeof(BITMAPINFOHEADER);
    bmInfo->bmiHeader.biWidth              = width;
    bmInfo->bmiHeader.biHeight             = -(int) height;
    bmInfo->bmiHeader.biPlanes             = 1;
    bmInfo->bmiHeader.biBitCount           = 32;
    bmInfo->bmiHeader.biCompression        = BI_RGB;
    bmInfo->bmiHeader.biSizeImage          = 0;
    bmInfo->bmiHeader.biXPelsPerMeter      = 72. / 0.0254; /* unused here */
    bmInfo->bmiHeader.biYPelsPerMeter      = 72. / 0.0254; /* unused here */
    bmInfo->bmiHeader.biClrUsed            = 0;
    bmInfo->bmiHeader.biClrImportant       = 0;

    hbm = CreateDIBSection(memHdc, bmInfo, DIB_RGB_COLORS, &data, NULL, 0);
    if (!hbm) {
        Tcl_Panic("CreateDIBSection");
    }
    SelectObject(memHdc, hbm);
    
    surface->bitmap = hbm;
    surface->width = width;
    surface->data = data;
    surface->width = width;
    surface->height = height;
	/* Windows bitmaps are padded to 16-bit (word) boundaries */
    surface->stride = 4*width;
    
    context->c = new PathC(memHdc);
    context->memHdc = memHdc;
    context->surface = surface;
    if (bmInfo) {
        ckfree((char *) bmInfo);
    }
    return (TkPathContext) context;
}

void TkPathPushTMatrix(TkPathContext ctx, TMatrix *m)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    if (m == NULL) {
        return;
    }
    context->c->PushTMatrix(m);
}

void TkPathSaveState(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
	context->c->SaveState();
}

void TkPathRestoreState(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
	context->c->RestoreState();
}

void TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->BeginPath(style);
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
    TkPathArcToUsingBezier(ctx, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void
TkPathRect(TkPathContext ctx, double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->AddRectangle((float) x, (float) y, (float) width, (float) height);
}

void
TkPathOval(TkPathContext ctx, double cx, double cy, double rx, double ry)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->AddEllipse((float) cx, (float) cy, (float) rx, (float) ry);
}

void
TkPathImage(TkPathContext ctx, Tk_Image image, Tk_PhotoHandle photo, 
        double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->DrawImage(photo, (float) x, (float) y, (float) width, (float) height);
}

void
TkPathClosePath(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->CloseFigure();
}

int
TkPathTextConfig(Tcl_Interp *interp, Tk_PathTextStyle *textStylePtr, char *utf8, void **customPtr)
{
	// @@@ We could think of having the FontFamily and Gdiplus::Font cached in custom.
    return TCL_OK;
}

void
TkPathTextDraw(TkPathContext ctx, Tk_PathStyle *style, Tk_PathTextStyle *textStylePtr, double x, double y, char *utf8, void *custom)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->DrawString(style, textStylePtr, (float) x, (float) y, utf8);
}

void
TkPathTextFree(Tk_PathTextStyle *textStylePtr, void *custom)
{
    /* Empty. */
}

PathRect
TkPathTextMeasureBbox(Tk_PathTextStyle *textStylePtr, char *utf8, void *custom)
{
    HDC memHdc;
    Tcl_DString ds, dsFont;
    Tcl_UniChar *uniPtr = NULL;
    PointF origin(0.0f, 0.0f);
    RectF bounds;
	PathRect r = {-1, -1, -1, -1};
    double ascent;
	Graphics *graphics = NULL;

	if (!sGdiplusStarted) {
		InitGDIplus();
    }
    memHdc = CreateCompatibleDC(NULL);
    /* @@@ I thought this was needed but seems not.
    HBITMAP bm = CreateCompatibleBitmap(memHdc, 10, 10);
    SelectObject(memHdc, bm);
    */
    graphics = new Graphics(memHdc);
    
	Tcl_DStringInit(&dsFont);
    FontFamily fontFamily((const WCHAR *)Tcl_UtfToUniCharDString(textStylePtr->fontFamily, -1, &dsFont));
	if (fontFamily.GetLastStatus() != Ok) {
		fontFamily.GenericSansSerif();
	}
	Gdiplus::Font font(&fontFamily, (float) textStylePtr->fontSize, FontStyleRegular, UnitPixel);
	if (font.GetLastStatus() != Ok) {
		// TODO
	}
	Tcl_DStringFree(&dsFont);
	Tcl_DStringInit(&ds);
	uniPtr = Tcl_UtfToUniCharDString(utf8, -1, &ds);
	graphics->MeasureString((const WCHAR *)uniPtr, Tcl_UniCharLen(uniPtr), &font, origin, &bounds);
	Tcl_DStringFree(&ds);
    ascent = font.GetSize() * 
           fontFamily.GetCellAscent(FontStyleRegular) / fontFamily.GetEmHeight(FontStyleRegular);
    r.x1 = 0.0;
    r.y1 = -ascent;
    r.x2 = bounds.Width;
    r.y2 = bounds.Height - ascent;
    delete graphics;
    // DeleteObject(bm);
    DeleteDC(memHdc);    
    return r;
}

void    	
TkPathSurfaceErase(TkPathContext ctx, double x, double y, double width, double height)
{
    // @@@ ???
}

void
TkPathSurfaceToPhoto(TkPathContext ctx, Tk_PhotoHandle photo)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    PathSurfaceGDIpRecord *surface = context->surface;
    Tk_PhotoImageBlock block;
    unsigned char *data;
    unsigned char *pixel;
    int width, height;
    int bytesPerRow;

    width = surface->width;
    height = surface->height;
    data = (unsigned char *)surface->data;
    bytesPerRow = surface->stride;

    Tk_PhotoGetImage(photo, &block);    
    pixel = (unsigned char *)ckalloc(height*bytesPerRow);
    if (gSurfaceCopyPremultiplyAlpha) {
        PathCopyBitsPremultipliedAlphaARGB(data, pixel, width, height, bytesPerRow);
    } else {
        PathCopyBitsARGB(data, pixel, width, height, bytesPerRow);
    }
    block.pixelPtr = pixel;
    block.width = width;
    block.height = height;
    block.pitch = bytesPerRow;
    block.pixelSize = 4;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;
    Tk_PhotoPutBlock(photo, &block, 0, 0, width, height, TK_PHOTO_COMPOSITE_OVERLAY);
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
    DeleteDC(context->memHdc);
    if (context->surface) {
        DeleteObject(context->surface->bitmap);
    }
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
int		
TkPathPixelAlign(void)
{
    return 1;
}

/* @@@ INCOMPLETE! We need to consider any padding as well. */

void TkPathPaintLinearGradient(TkPathContext ctx, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->FillLinearGradient(bbox, fillPtr, fillRule);
}

void
TkPathPaintRadialGradient(TkPathContext ctx, PathRect *bbox, RadialGradientFill *fillPtr, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    context->c->FillRadialGradient(bbox, fillPtr, fillRule);
}


