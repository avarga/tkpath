/*
 * tkPath.h --
 *
 *		This file implements a path drawing model
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#ifndef INCLUDED_TKPATH_H
#define INCLUDED_TKPATH_H

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The enum below defines the valid types for the PathAtom's.
 */

typedef enum {
    PATH_ATOM_M = 'M',
    PATH_ATOM_L = 'L',
    PATH_ATOM_A = 'A',
    PATH_ATOM_Q = 'Q',
    PATH_ATOM_C = 'C',
    PATH_ATOM_Z = 'Z'
} PathAtomType;

enum {
    PATH_NEXT_ERROR,
    PATH_NEXT_INSTRUCTION,
    PATH_NEXT_OTHER
};

typedef struct PathPoint {
    double x;
    double y;
} PathPoint;

typedef struct PathRect {
    double x1;
    double y1;
    double x2;
    double y2;
} PathRect;

/*
 * The transformation matrix:
 *		| a  b  0 |
 *		| c  d  0 |
 *		| tx ty 1 |
 */
 
typedef struct TMatrix {
    double a, b, c, d;
    double tx, ty;
} TMatrix;

static const TMatrix kUnitTMatrix = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

/*
 * Records used for parsing path to a linked list of primitive 
 * drawing instructions.
 *
 * PathAtom: vaguely modelled after Tk_Item. Each atom has a PathAtom record
 * in its first position, padded with type specific data.
 */
 
typedef struct PathAtom {
    PathAtomType type;			/* Type of PathAtom. */
    struct PathAtom *nextPtr;	/* Next PathAtom along the path. */
} PathAtom;

/*
 * Records for gradient fills.
 */
 
typedef struct GradientStop {
    double offset;
    XColor *color;
    double opacity;
} GradientStop;

typedef struct LinearGradientFill {
    PathRect transition;	/* Actually not a proper rect but a vector. */
    int method;
    int fillRule;			/* Not yet used. */
    int nstops;
    GradientStop **stops;	/* Array of pointers to GradientStop. */
} LinearGradientFill;

typedef struct RadialGradientFill {
    double centerX;
    double centerY;
    double rad;
    int nstops;
    GradientStop **stops;
} RadialGradientFill;

/* 
 * Information used for parsing configuration options.
 * Mask bits for options changed.
 */
 
enum {
    PATH_STYLE_OPTION_FILL              	= (1L << 0),
    PATH_STYLE_OPTION_FILLGRADIENT        	= (1L << 1),
    PATH_STYLE_OPTION_FILLOFFSET        	= (1L << 2),
    PATH_STYLE_OPTION_FILLOPACITY    		= (1L << 3),
    PATH_STYLE_OPTION_FILLRULE         		= (1L << 4),
    PATH_STYLE_OPTION_FILLSTIPPLE      		= (1L << 5),
    PATH_STYLE_OPTION_MATRIX              	= (1L << 6),
    PATH_STYLE_OPTION_STROKE           		= (1L << 7),
    PATH_STYLE_OPTION_STROKEDASHARRAY    	= (1L << 8),
    PATH_STYLE_OPTION_STROKELINECAP        	= (1L << 9),
    PATH_STYLE_OPTION_STROKELINEJOIN       	= (1L << 10),
    PATH_STYLE_OPTION_STROKEMITERLIMIT     	= (1L << 11),
    PATH_STYLE_OPTION_STROKEOFFSET        	= (1L << 12),
    PATH_STYLE_OPTION_STROKEOPACITY	       	= (1L << 13),
    PATH_STYLE_OPTION_STROKESTIPPLE     	= (1L << 14),
    PATH_STYLE_OPTION_STROKEWIDTH        	= (1L << 15)
};

typedef struct Tk_PathStyle {
	Tk_OptionTable optionTable;	/* Not used for canvas. */
	Tk_Uid name;				/* Not used for canvas. */
    int mask;					/* Bits set for actual options modified. */
    GC strokeGC;				/* Graphics context for stroke. */
    XColor *strokeColor;		/* Stroke color. */
    double strokeWidth;			/* Width of stroke. */
    double strokeOpacity;
    int offset;					/* Dash offset */
    Tk_Dash dash;				/* Dash pattern. */
    Tk_TSOffset strokeTSOffset;	/* Stipple offset for stroke. */
    Pixmap strokeStipple;		/* Stroke Stipple pattern. */
    int capStyle;				/* Cap style for stroke. */
    int joinStyle;				/* Join style for stroke. */
    double miterLimit;
    char *linearGradientStrokeName; /* Unused; for the future. */

    GC fillGC;					/* Graphics context for filling path. */
    XColor *fillColor;			/* Foreground color for filling. */
    double fillOpacity;
    Tk_TSOffset fillTSOffset;	/* Stipple offset for filling. */
    Pixmap fillStipple;			/* Stipple bitmap for filling path. */
    int fillRule;				/* WindingRule or EvenOddRule. */
    char *gradientFillName;  	/* This is the *name* of the linear 
                                 * gradient fill. No fill record since
                                 * bad idea duplicate pointers.
                                 * Look up each time. */
    TMatrix *matrix;			/*  a  b   default (NULL): 1 0
                                    c  d				   0 1
                                    tx ty 				   0 0 */
    char *null;   				/* Just a placeholder for not yet implemented stuff. */ 
} Tk_PathStyle;

/*
 * Functions that process lists and atoms.
 */
 
int			TkPathParseToAtoms(Tcl_Interp *interp, Tcl_Obj *listObjPtr, PathAtom **atomPtrPtr, int *lenPtr);
void		TkPathFreeAtoms(PathAtom *pathAtomPtr);
int			TkPathNormalize(Tcl_Interp *interp, PathAtom *atomPtr, Tcl_Obj **listObjPtrPtr);
int			TkPathMakePath(Drawable drawable, PathAtom *atomPtr, Tk_PathStyle *stylePtr);

/*
 * Stroke, fill, clip etc.
 */
 
void		TkPathClipToPath(Drawable d, int fillRule);
void		TkPathReleaseClipToPath(Drawable d);
void		TkPathStroke(Drawable d, Tk_PathStyle *style);
void		TkPathFill(Drawable d, Tk_PathStyle *style);
void        TkPathFillAndStroke(Drawable d, Tk_PathStyle *style);
int			TkPathGetCurrentPosition(Drawable d, PathPoint *ptPtr);
int 		TkPathBoundingBox(PathRect *rPtr);
void		TkPathPaintLinearGradient(Drawable d, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule);
void    	TkPathFree(Drawable d);
int			TkPathDrawingDestroysPath();

/*
 * Utilities for creating and deleting Tk_PathStyles.
 */
 
void 		Tk_CreatePathStyle(Tk_PathStyle *style);
void 		Tk_DeletePathStyle(Display *display, Tk_PathStyle *style);



/*
 * end block for C++
 */
    
#ifdef __cplusplus
}
#endif

#endif      // INCLUDED_TKPATH_H


