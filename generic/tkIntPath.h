/*
 * tkIntPath.h --
 *
 *		Header file for the internals of the tkpath package.
 *
 * Copyright (c) 2005  Mats Bengtsson
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
# Ê Ê	include <sys/types.h>
#	endif
#	ifdef HAVE_SYS_PARAM_H
# Ê Ê	include <sys/param.h>
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

#define MIN(a, b) 	(((a) < (b)) ? (a) : (b))
#define MAX(a, b) 	(((a) > (b)) ? (a) : (b))
#define ABS(a)    	(((a) >= 0)  ? (a) : -1*(a))
#define PI 3.14159265358979323846
#define DEGREES_TO_RADIANS (PI/180.0)
#define RADIANS_TO_DEGREES (180.0/PI)

/* Takes a double and aligns it to the closest pixels center.
 * This is useful when not antialiasing since some systems 
 * (CoreGraphics on MacOSX 10.2 and cairo) consistantly gives 2 pixel
 * line widths when 1 is expected.
 * This works fine on verical and horizontal lines using CG but terrible else!
 * Best to use this on tcl level.
 */
#define ALIGN_TO_PIXEL(x) 	(gUseAntiAlias ? (x) : (((int) x) + 0.5))

/*
 * So far we use a fixed number of straight line segments when
 * doing various things, but it would be better to use the de Castlejau
 * algorithm to iterate these segments.
 */
static int kPathNumSegmentsCurveTo    = 18;
static int kPathNumSegmentsQuadBezier = 12;
static int kPathNumSegmentsMax		  = 18;

static const TMatrix kPathUnitTMatrix = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

/* These MUST be kept in sync with methodST ! */
enum {
    kPathGradientMethodPad = 		0L,
    kPathGradientMethodRepeat,
    kPathGradientMethodReflect
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

/*
 * The actual path drawing commands which are all platform specific.
 */
 
void		TkPathInit(Display *display, Drawable d);
void		TkPathBeginPath(Drawable d, Tk_PathStyle *stylePtr);
void    	TkPathEndPath(Drawable d);
void		TkPathMoveTo(Drawable d, double x, double y);
void		TkPathLineTo(Drawable d, double x, double y);
void		TkPathArcTo(Drawable d, double rx, double ry, double angle, 
                    char largeArcFlag, char sweepFlag, double x, double y);
void		TkPathQuadBezier(Drawable d, double ctrlX, double ctrlY, double x, double y);
void		TkPathCurveTo(Drawable d, double ctrlX1, double ctrlY1, 
                    double ctrlX2, double ctrlY2, double x, double y);
void		TkPathArcToUsingBezier(Drawable d, double rx, double ry, 
                    double phiDegrees, char largeArcFlag, char sweepFlag, 
                    double x2, double y2);
void		TkPathClosePath(Drawable d);

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
void		PathLinearGradientInit(Tcl_Interp* interp);
int 		StyleObjCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* CONST objv[]);
int			PathStyleHaveWithName(CONST char *name);
int			HaveLinearGradientStyleWithName(CONST char *name);
void		PathStyleMergeStyles(Tk_Window tkwin, Tk_PathStyle *stylePtr, CONST char *styleName);
int			PathPolyLineToArea(double *polyPtr, int numPoints, register double *rectPtr);
double		PathThickPolygonToPoint(int joinStyle, int capStyle, double width, 
                    int isclosed, double *polyPtr, int numPoints, double *pointPtr);
double		PathPolygonToPointEx(double *polyPtr, int numPoints, double *pointPtr, 
                    int *intersectionsPtr, int *nonzerorulePtr);


/*
 * end block for C++
 */
    
#ifdef __cplusplus
}
#endif

#endif      // INCLUDED_TKINTPATH_H

