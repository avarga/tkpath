/*
 * tkIntPath.h --
 *
 *		Header file for the internals of the tkpath package.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * $Id$
 */

#ifndef INCLUDED_TKINTPATH_H
#define INCLUDED_TKINTPATH_H

#include "tkPath.h"

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * From tclInt.h version 1.118.2.11
 * Ensure WORDS_BIGENDIAN is defined correcly:
 * Needs to happen here in addition to configure to work with
 * fat compiles on Darwin (i.e. ppc and i386 at the same time).
 */
 
#ifndef WORDS_BIGENDIAN
#	ifdef HAVE_SYS_TYPES_H
#		include <sys/types.h>
#	endif
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#   ifdef BYTE_ORDER
#		ifdef BIG_ENDIAN
#			if BYTE_ORDER == BIG_ENDIAN
#				define WORDS_BIGENDIAN
#			endif
#		endif
#		ifdef LITTLE_ENDIAN
#			if BYTE_ORDER == LITTLE_ENDIAN
#				undef WORDS_BIGENDIAN
#			endif
#		endif
#	endif
#endif

#ifndef MIN
#	define MIN(a, b) 	(((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#	define MAX(a, b) 	(((a) > (b)) ? (a) : (b))
#endif
#ifndef ABS
#	define ABS(a)    	(((a) >= 0)  ? (a) : -1*(a))
#endif
#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#define DEGREES_TO_RADIANS (M_PI/180.0)
#define RADIANS_TO_DEGREES (180.0/M_PI)

/* Takes a double and aligns it to the closest pixels center.
 * This is useful when not antialiasing since some systems 
 * (CoreGraphics on MacOSX 10.2 and cairo) consistantly gives 2 pixel
 * line widths when 1 is expected.
 * This works fine on verical and horizontal lines using CG but terrible else!
 * Best to use this on tcl level.
 */
#define ALIGN_TO_PIXEL(x) 	(gUseAntiAlias ? (x) : (((int) x) + 0.5))

/* This can be useful to estimate the segmentation detail necessary.
 * A conservative measure.
 */
#define TMATRIX_ABS_MAX(mPtr)		MAX(fabs(mPtr->a), MAX(fabs(mPtr->b), MAX(fabs(mPtr->c), fabs(mPtr->d))))

/* This can be used for simplifying Area and Point functions.
 */
#define TMATRIX_IS_RECTILINEAR(mPtr)   	(fabs(mPtr->b) == 0.0) && (fabs(mPtr->c) == 0.0)

#define TMATRIX_DETERMINANT(mPtr)		(mPtr->a * mPtr->d - mPtr->c * mPtr->d)

/*
 * So far we use a fixed number of straight line segments when
 * doing various things, but it would be better to use the de Castlejau
 * algorithm to iterate these segments.
 */
#define kPathNumSegmentsCurveTo     	18
#define kPathNumSegmentsQuadBezier 		12
#define kPathNumSegmentsMax		  		18
#define kPathNumSegmentsEllipse         48

#define kPathUnitTMatrix  {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}

extern int gUseAntiAlias;

/* These MUST be kept in sync with methodST and unitsST! */
enum {
    kPathGradientMethodPad = 		0L,
    kPathGradientMethodRepeat,
    kPathGradientMethodReflect
};
enum {
    kPathGradientUnitsBoundingBox =	0L,
    kPathGradientUnitsUserSpace
};

enum {
    kPathArcOK,
    kPathArcLine,
    kPathArcSkip
};

typedef struct PathBox {
    double x;
    double y;
    double width;
    double height;
} PathBox;

typedef struct CentralArcPars {
    double cx;
    double cy;
    double rx;
    double ry;
    double theta1;
    double dtheta;
    double phi;
} CentralArcPars;

typedef struct LookupTable {
    int from;
    int to;
} LookupTable;

/*
 * Records used for parsing path to a linked list of primitive 
 * drawing instructions.
 *
 * PathAtom: vaguely modelled after Tk_Item. Each atom has a PathAtom record
 * in its first position, padded with type specific data.
 */

typedef struct MoveToAtom {
    PathAtom pathAtom;			/* Generic stuff that's the same for all
                                 * types.  MUST BE FIRST IN STRUCTURE. */
    double x;
    double y;
} MoveToAtom;

typedef struct LineToAtom {
    PathAtom pathAtom;
    double x;
    double y;
} LineToAtom;

typedef struct ArcAtom {
    PathAtom pathAtom;
    double radX;
    double radY;
    double angle;		/* In degrees! */
    char largeArcFlag;
    char sweepFlag;
    double x;
    double y;
} ArcAtom;

typedef struct QuadBezierAtom {
    PathAtom pathAtom;
    double ctrlX;
    double ctrlY;
    double anchorX;
    double anchorY;
} QuadBezierAtom;

typedef struct CurveToAtom {
    PathAtom pathAtom;
    double ctrlX1;
    double ctrlY1;
    double ctrlX2;
    double ctrlY2;
    double anchorX;
    double anchorY;
} CurveToAtom;

typedef struct CloseAtom {
    PathAtom pathAtom;
    double x;
    double y;
} CloseAtom;

typedef struct EllipseAtom {
    PathAtom pathAtom;
    double cx;
    double cy;
    double rx;
    double ry;
} EllipseAtom;

typedef struct RectAtom {
    PathAtom pathAtom;
    double x;
    double y;
    double width;
    double height;
} RectAtom;

/*
 * Flags for 'PathStyleMergeStyles'.
 */
 
enum {
    kPathMergeStyleNotFill = 		0L,
    kPathMergeStyleNotStroke
};

/*
 * The actual path drawing commands which are all platform specific.
 */
 
TkPathContext		TkPathInit(Tk_Window tkwin, Drawable d);
void		TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *stylePtr);
void    	TkPathEndPath(TkPathContext ctx);
void		TkPathMoveTo(TkPathContext ctx, double x, double y);
void		TkPathLineTo(TkPathContext ctx, double x, double y);
void		TkPathArcTo(TkPathContext ctx, double rx, double ry, double angle, 
                    char largeArcFlag, char sweepFlag, double x, double y);
void		TkPathQuadBezier(TkPathContext ctx, double ctrlX, double ctrlY, double x, double y);
void		TkPathCurveTo(TkPathContext ctx, double ctrlX1, double ctrlY1, 
                    double ctrlX2, double ctrlY2, double x, double y);
void		TkPathArcToUsingBezier(TkPathContext ctx, double rx, double ry, 
                    double phiDegrees, char largeArcFlag, char sweepFlag, 
                    double x2, double y2);
void		TkPathRect(TkPathContext ctx, double x, double y, double width, double height);
void		TkPathOval(TkPathContext ctx, double cx, double cy, double rx, double ry);
void		TkPathClosePath(TkPathContext ctx);
void		TkPathImage(TkPathContext ctx, Tk_Image image, Tk_PhotoHandle photo, 
                    double x, double y, double width, double height);

/*
 * General path drawing using linked list of path atoms.
 */
void		TkPathDrawPath(Tk_Window tkwin, Drawable drawable,
                    PathAtom *atomPtr, Tk_PathStyle *stylePtr, TMatrix *mPtr,			
                    PathRect *bboxPtr);
void		TkPathPaintPath(TkPathContext context, PathAtom *atomPtr,
                    Tk_PathStyle *stylePtr, PathRect *bboxPtr);

/* Various stuff. */
int 		TableLookup(LookupTable *map, int n, int from);
void		PathParseDashToArray(Tk_Dash *dash, double width, int *len, float **arrayPtrPtr);
void 		PathApplyTMatrix(TMatrix *m, double *x, double *y);
void 		PathApplyTMatrixToPoint(TMatrix *m, double in[2], double out[2]);
void		PathInverseTMatrix(TMatrix *m, TMatrix *mi);

int			ObjectIsEmpty(Tcl_Obj *objPtr);
int			PathGetTMatrix(Tcl_Interp* interp, CONST char *list, TMatrix *matrixPtr);
int			PathGetTclObjFromTMatrix(Tcl_Interp* interp, TMatrix *matrixPtr,
                    Tcl_Obj **listObjPtrPtr);

int			EndpointToCentralArcParameters(
                    double x1, double y1, double x2, double y2,	/* The endpoints. */
                    double rx, double ry,				/* Radius. */
                    double phi, char largeArcFlag, char sweepFlag,
                    double *cxPtr, double *cyPtr, 			/* Out. */
                    double *rxPtr, double *ryPtr,
                    double *theta1Ptr, double *dthetaPtr);

int 		PathGenericCmdDispatcher( 
                    Tcl_Interp* interp,
                    int objc,
                    Tcl_Obj* CONST objv[],
                    char *baseName,
                    int *baseNameUIDPtr,
                    Tcl_HashTable *hashTablePtr,
                    Tk_OptionTable optionTable,
                    char *(*createAndConfigProc)(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]),
                    void (*configNotifyProc)(char *recordPtr, int mask, int objc, Tcl_Obj *CONST objv[]),
                    void (*freeProc)(Tcl_Interp *interp, char *recordPtr));
void		PathStyleInit(Tcl_Interp* interp);
void		PathGradientInit(Tcl_Interp* interp);
int 		StyleObjCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* CONST objv[]);
int			PathStyleHaveWithName(CONST char *name);
int			HaveGradientStyleWithName(CONST char *name);
void		PathStyleMergeStyles(Tk_Window tkwin, Tk_PathStyle *stylePtr, CONST char *styleName, long flags);

void		PathPaintGradientFromName(TkPathContext ctx, PathRect *bbox, char *name, int fillRule);


/*
 * end block for C++
 */
    
#ifdef __cplusplus
}
#endif

#endif      // INCLUDED_TKINTPATH_H

