/*
 * tkCanvPath.c --
 *
 *	This file implements a path canvas item modelled after its
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <tcl.h>
#include <tk.h>
#include <tkInt.h>
#include "tkPort.h"
#include "tkCanvas.h"
#include "tkPath.h"
#include "tkIntPath.h"

static double kStrokeThicknessLimit = 4.0;

static int kNumSegmentsCurveTo = 12;
static int kNumSegmentsQuadBezier = 12;

#define MAX_NUM_STATIC_SEGMENTS  2000
/* @@@ Should this be moved inside the function instead? */
static double staticSpace[2*MAX_NUM_STATIC_SEGMENTS];

int gDebugLevel = 2;
Tcl_Interp *gInterp;

#define PATH_DEBUG 0

/* Values for the PathItem's flag. */

enum {
    kPathItemNeedNewNormalizedPath                     = (1L << 0)
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PathItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Outline outline;		/* Outline structure */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    Tcl_Obj *pathObjPtr;	/* The object containing the path definition. */
    int pathLen;
    Tcl_Obj *normPathObjPtr;/* The object containing the normalized path. */
    PathAtom *atomPtr;
    PathRect bareBbox;		/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    PathRect totalBbox;		/* Bounding box including stroke.
                             * Untransformed coordinates. */
    int maxNumSegments;		/* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    long flags;				/* Various flags, see enum. */
} PathItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePathBbox(Tk_Canvas canvas, PathItem *pathPtr);
static int		ConfigurePath(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePath(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable dst,
                        int x, int y, int width, int height);
static int		PathCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PathToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PathToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PathToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePath(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);

/* For processing custom options. */

static int		FillRuleParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
static char *	FillRulePrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
static int		LinearGradientParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
static char *	LinearGradientPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
static int		MatrixParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
static char *	MatrixPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
static int		StyleParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
static char *	StylePrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);

/* Support functions. */

static int		GetSubpathMaxNumSegments(PathAtom *atomPtr);
static void		MakeSubPathSegments(PathAtom **atomPtrPtr, double *polyPtr, 
                        int *numPointsPtr, int *numStrokesPtr, TMatrix *matrixPtr);


/* From tkPathCopyTk.c */
extern int		PathTk_CanvasTagsParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
extern char *	PathTk_CanvasTagsPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
extern int		PathTkCanvasDashParseProc(ClientData clientData, Tcl_Interp *interp,
                        Tk_Window tkwin, CONST char *value,
                        char *widgRec, int offset);

extern char *	PathTkCanvasDashPrintProc(ClientData clientData,
                        Tk_Window tkwin, char *widgRec, int offset,
                        Tcl_FreeProc **freeProcPtr);
extern int		PathTkPixelParseProc(ClientData clientData, Tcl_Interp *interp,
                        Tk_Window tkwin, CONST char *value,
                        char *widgRec, int offset);
extern char *	PathTkPixelPrintProc(ClientData clientData, Tk_Window tkwin,    
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
extern int		PathTkStateParseProc(ClientData clientData, Tcl_Interp *interp,
                        Tk_Window tkwin, CONST char *value,    
                        char *widgRec, int offset);
extern char *	PathTkStatePrintProc(ClientData clientData, Tk_Window tkwin,
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
extern int		PathTkOffsetParseProc(ClientData clientData, Tcl_Interp *interp,
                        Tk_Window tkwin, CONST char *value,    
                        char *widgRec, int offset);
extern char *	PathTkOffsetPrintProc(ClientData clientData, Tk_Window tkwin,
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
    

extern int 		LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                        int objc, Tcl_Obj* CONST objv[]);
extern void		PathPaintLinearGradientFromName(Drawable d, PathRect *bbox, char *name, int fillRule);

    
/*
 * Information used for parsing configuration specs.  If you change any
 * of the default strings, be sure to change the corresponding default
 * values in CreatePath.
 */

static Tk_CustomOption stateOption = {
    (Tk_OptionParseProc *) PathTkStateParseProc,
    PathTkStatePrintProc, 
    (ClientData) 2
};
static Tk_CustomOption tagsOption = {
    (Tk_OptionParseProc *) PathTk_CanvasTagsParseProc,
    PathTk_CanvasTagsPrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption dashOption = {
    (Tk_OptionParseProc *) PathTkCanvasDashParseProc,
    PathTkCanvasDashPrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption offsetOption = {
    (Tk_OptionParseProc *) PathTkOffsetParseProc,
    PathTkOffsetPrintProc,
    (ClientData) (TK_OFFSET_RELATIVE|TK_OFFSET_INDEX)
};
static Tk_CustomOption pixelOption = {
    (Tk_OptionParseProc *) PathTkPixelParseProc,
    PathTkPixelPrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption fillRuleOption = {
    (Tk_OptionParseProc *) FillRuleParseProc,
    FillRulePrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption linGradOption = {
    (Tk_OptionParseProc *) LinearGradientParseProc,
    LinearGradientPrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption matrixOption = {
    (Tk_OptionParseProc *) MatrixParseProc,
    MatrixPrintProc, 
    (ClientData) NULL
};
static Tk_CustomOption styleOption = {
    (Tk_OptionParseProc *) StyleParseProc,
    StylePrintProc, 
    (ClientData) NULL
};


static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
            "", Tk_Offset(PathItem, style.fillColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-fillgradient", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, style.gradientFillName),
            TK_CONFIG_NULL_OK, &linGradOption},
    {TK_CONFIG_CUSTOM, "-filloffset", (char *) NULL, (char *) NULL,
            "0,0", Tk_Offset(PathItem, style.fillTSOffset),
            TK_CONFIG_DONT_SET_DEFAULT, &offsetOption},
    {TK_CONFIG_DOUBLE, "-fillopacity", (char *) NULL, (char *) NULL,
            "1.0", Tk_Offset(PathItem, style.fillOpacity), 0},
    {TK_CONFIG_CUSTOM, "-fillrule", (char *) NULL, (char *) NULL,
            "nonzero", Tk_Offset(PathItem, style.fillRule),
            TK_CONFIG_DONT_SET_DEFAULT, &fillRuleOption},
    {TK_CONFIG_BITMAP, "-fillstipple", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, style.fillStipple),
            TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-matrix", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, style.matrixPtr),
            TK_CONFIG_NULL_OK, &matrixOption},
    {TK_CONFIG_CUSTOM, "-state", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(Tk_Item, state), TK_CONFIG_NULL_OK,
            &stateOption},
    {TK_CONFIG_COLOR, "-stroke", (char *) NULL, (char *) NULL,
            "black", Tk_Offset(PathItem, style.strokeColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-strokedasharray", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, style.dash),
            TK_CONFIG_NULL_OK, &dashOption},
    {TK_CONFIG_CAP_STYLE, "-strokelinecap", (char *) NULL, (char *) NULL,
            "butt", Tk_Offset(PathItem, style.capStyle), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_JOIN_STYLE, "-strokelinejoin", (char *) NULL, (char *) NULL,
            "round", Tk_Offset(PathItem, style.joinStyle), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-strokemiterlimit", (char *) NULL, (char *) NULL,
            "4.0", Tk_Offset(PathItem, style.miterLimit), 0},
    {TK_CONFIG_CUSTOM, "-strokeoffset", (char *) NULL, (char *) NULL,
            "0,0", Tk_Offset(PathItem, style.strokeTSOffset),
            TK_CONFIG_DONT_SET_DEFAULT, &offsetOption},
    {TK_CONFIG_DOUBLE, "-strokeopacity", (char *) NULL, (char *) NULL,
            "1.0", Tk_Offset(PathItem, style.strokeOpacity), 0},
    {TK_CONFIG_BITMAP, "-strokestipple", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, style.strokeStipple),
            TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-strokewidth", (char *) NULL, (char *) NULL,
            "1.0", Tk_Offset(PathItem, style.strokeWidth),
            TK_CONFIG_DONT_SET_DEFAULT, &pixelOption},
    {TK_CONFIG_CUSTOM, "-style", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PathItem, styleName),
            TK_CONFIG_DONT_SET_DEFAULT, &styleOption},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
            (char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
            (char *) NULL, 0, 0}
};

/*
 * The structures below defines the line item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPathType = {
    "path",					/* name */
    sizeof(PathItem),		/* itemSize */
    CreatePath,				/* createProc */
    configSpecs,			/* configSpecs */
    ConfigurePath,			/* configureProc */
    PathCoords,				/* coordProc */
    DeletePath,				/* deleteProc */
    DisplayPath,			/* displayProc */
    TK_CONFIG_OBJS,			/* flags */
    PathToPoint,			/* pointProc */
    PathToArea,				/* areaProc */
    PathToPostscript,		/* postscriptProc */
    ScalePath,				/* scaleProc */
    TranslatePath,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,	/* nextPtr */
};

/* This one seems missing. */
#if 0
static Tcl_Interp *
Tk_CanvasInterp(Tk_Canvas canvas)
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    return canvasPtr->interp;
}
#endif


void
DebugPrintf(Tcl_Interp *interp, int level, char *fmt, ...)
{
	va_list		args;
	char		tmpstr[256];
	
	if (level > gDebugLevel) {
		return;
	}
	va_start( args, fmt );
	vsprintf( tmpstr, fmt, args );	
    Tcl_VarEval( interp, "puts \"", tmpstr, "\"", (char *) NULL );
	va_end (args );
}

/*
 * A bunch of custum option processing functions needed.
 */
 
/*
 *--------------------------------------------------------------
 *
 * FillRuleParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-fillrule" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
FillRuleParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    int c;
    size_t length;
    
    register int *fillRulePtr = (int *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        *fillRulePtr = WindingRule;
        return TCL_OK;
    }

    c = value[0];
    length = strlen(value);

    if ((c == 'n') && (strncmp(value, "nonzero", length) == 0)) {
        *fillRulePtr = WindingRule;
        return TCL_OK;
    }
    if ((c == 'e') && (strncmp(value, "evenodd", length) == 0)) {
        *fillRulePtr = EvenOddRule;
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad value \"", value, 
            "\": must be \"nonzero\" or \"evenodd\"",
	    (char *) NULL);
        *fillRulePtr = WindingRule;
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * FillRulePrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-fillrule"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static char *
FillRulePrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register int *fillRulePtr = (int *) (widgRec + offset);
    *freeProcPtr = NULL;

    if (*fillRulePtr == WindingRule) {
        return "nonzero";
    } else if (*fillRulePtr == EvenOddRule) {
        return "evenodd";
    } else {
        return "";
    }
}

/*
 *--------------------------------------------------------------
 *
 * LinearGradientParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-lineargradient" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
LinearGradientParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;    
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
        if (HaveLinearGradientStyleWithName(value) != TCL_OK) {
            Tcl_AppendResult(interp, "bad value \"", value, 
                    "\": does not exist",
                    (char *) NULL);
            return TCL_ERROR;
        } else {
            new = (char *) ckalloc((unsigned) (strlen(value) + 1));
            strcpy(new, value);
        }
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LinearGradientPrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-lineargradient"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static char *
LinearGradientPrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    char *result;
    register char *ptr = (char *) (widgRec + offset);

    result = (*(char **) ptr);
    if (result == NULL) {
        result = "";
    }
    return result;
}


/*
 *--------------------------------------------------------------
 *
 * StyleParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-style" option.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
StyleParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;    
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
    
        /* 
         * We only check that the style name exist here and
         * do the processing after configuration.
         */
        if (PathStyleHaveWithName(value) != TCL_OK) {
            Tcl_AppendResult(interp, "bad value \"", value, 
                    "\": does not exist",
                    (char *) NULL);
            return TCL_ERROR;
        } else {
            new = (char *) ckalloc((unsigned) (strlen(value) + 1));
            strcpy(new, value);
        }
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * StylePrintProc --
 *
 *		This procedure is invoked by the Tk configuration code
 *		to produce a printable string for the "-style"
 *		configuration option.
 *
 * Results:
 *		The return value is a string describing the state for
 *		the item referred to by "widgRec".  In addition, *freeProcPtr
 *		is filled in with the address of a procedure to call to free
 *		the result string when it's no longer needed (or NULL to
 *		indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static char *
StylePrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    char *result;
    register char *ptr = (char *) (widgRec + offset);

    result = (*(char **) ptr);
    if (result == NULL) {
        result = "";
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * MatrixParseProc --
 *
 *		This procedure is invoked during option processing to handle
 *		the "-matrix" option. It translates the (string) option 
 *		into a double array.
 *
 * Results:
 *		A standard Tcl return value.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
MatrixParseProc(
    ClientData clientData,		/* some flags.*/
    Tcl_Interp *interp,			/* Used for reporting errors. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    CONST char *value,			/* Value of option. */
    char *widgRec,			/* Pointer to record for item. */
    int offset)				/* Offset into item. */
{
    char *old, *new;
    TMatrix *matrixPtr;
    register char *ptr = (char *) (widgRec + offset);

    if(value == NULL || *value == 0) {
        new = NULL;
    } else {
        matrixPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
        if (PathGetTMatrix(interp, value, matrixPtr) != TCL_OK) {
            ckfree((char *) matrixPtr);
            return TCL_ERROR;
        }
        new = (char *) matrixPtr;
    }
    old = *((char **) ptr);
    if (old != NULL) {
        ckfree(old);
    }
    
    /* Note: the _value_ of the address is in turn a pointer to string. */
    *((char **) ptr) = new;
    
    return TCL_OK;
}

static char *
MatrixPrintProc(
    ClientData clientData,		/* Ignored. */
    Tk_Window tkwin,			/* Window containing canvas widget. */
    char *widgRec,			/* Pointer to record for item. */
    int offset,				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr)		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    char *buffer, *str;
    int len;
    TMatrix *matrixPtr;
    Tcl_Obj *listObj;
    register char *ptr = (char *) (widgRec + offset);

    *freeProcPtr = TCL_DYNAMIC;

    matrixPtr = (*(TMatrix **) ptr); 
    PathGetTclObjFromTMatrix(NULL, matrixPtr, &listObj);
    str = Tcl_GetStringFromObj(listObj, &len);
    buffer = (char *) ckalloc((unsigned int) (len + 1));
    strcpy(buffer, str);
    Tcl_DecrRefCount(listObj);

    return buffer;
}

/*
 *--------------------------------------------------------------
 *
 * MakeCanvasPath
 *
 *		Defines the path in the canvas using the PathItem.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side effects:
 *		Defines the current path in drawable.
 *
 *--------------------------------------------------------------
 */

int
MakeCanvasPath(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    PathItem *pathPtr,
    Drawable drawable)			/* Pixmap or window in which to draw
					 * item. */
{
    short drawableX, drawableY, drawableX1, drawableY1, drawableX2, drawableY2;
    PathAtom *atomPtr = pathPtr->atomPtr;
    
    TkPathBeginPath(drawable, &(pathPtr->style));

    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: { 
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                Tk_CanvasDrawableCoords(canvas, move->x, move->y, 
                        &drawableX, &drawableY); 
                TkPathMoveTo(drawable, drawableX, drawableY);
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                Tk_CanvasDrawableCoords(canvas, line->x, line->y, 
                        &drawableX, &drawableY); 
                TkPathLineTo(drawable, drawableX, drawableY);
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;

                Tk_CanvasDrawableCoords(canvas, 
                        arc->x, arc->y, 
                        &drawableX2, &drawableY2); 
                TkPathArcTo(drawable, arc->radX, arc->radY, 
                        arc->angle, arc->largeArcFlag, arc->sweepFlag,
                        drawableX2, drawableY2);
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                Tk_CanvasDrawableCoords(canvas, 
                        quad->ctrlX, quad->ctrlY, 
                        &drawableX1, &drawableY1); 
                Tk_CanvasDrawableCoords(canvas, 
                        quad->anchorX, quad->anchorY, 
                        &drawableX, &drawableY); 
                TkPathQuadBezier(drawable, drawableX1, drawableY1, 
                        drawableX, drawableY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;
                
                Tk_CanvasDrawableCoords(canvas, 
                        curve->ctrlX1, curve->ctrlY1, 
                        &drawableX1, &drawableY1); 
                Tk_CanvasDrawableCoords(canvas, 
                        curve->ctrlX2, curve->ctrlY2, 
                        &drawableX2, &drawableY2); 
                Tk_CanvasDrawableCoords(canvas, 
                        curve->anchorX, curve->anchorY, 
                        &drawableX, &drawableY); 
                TkPathCurveTo(drawable, drawableX1, drawableY1,
                        drawableX2, drawableY2,
                        drawableX, drawableY);
                break;
            }
            case PATH_ATOM_Z: {
                TkPathClosePath(drawable);
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    TkPathEndPath(drawable);
    return TCL_OK;
}

static PathRect
NewEmptyPathRect(void)
{
    PathRect r;
    
    r.x1 = 1.0e36;
    r.y1 = 1.0e36;
    r.x2 = -1.0e36;
    r.y2 = -1.0e36;
    return r;
}

static int
IsPathRectEmpty(PathRect *r)
{
    if ((r->x2 > r->x1) && (r->y2 > r->y1)) {
        return 0;
    } else {
        return 1;
    }
}

static void
TranslatePathRect(PathRect *r, double deltaX, double deltaY)
{
    r->x1 += deltaX;
    r->x2 += deltaX;
    r->y1 += deltaY;
    r->y2 += deltaY;
}

/* Be sure rect is not empty (see above) before doing this. */
static void
NormalizePathRect(PathRect *r)
{
    double min, max;

    min = MIN(r->x1, r->x2);
    max = MAX(r->x1, r->x2);
    r->x1 = min;
    r->x2 = max;
    min = MIN(r->y1, r->y2);
    max = MAX(r->y1, r->y2);
    r->y1 = min;
    r->y2 = max;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_ConfigStrokePathStyleGC
 *
 *		This procedure should be called in the canvas object
 *		during the configure command. The graphics context
 *		description in gcValues is updated according to the
 *		information in the dash structure, as far as possible.
 *
 * Results:
 *		The return-value is a mask, indicating which
 *		elements of gcValues have been updated.
 *		0 means there is no outline.
 *
 * Side effects:
 *		GC information in gcValues is updated.
 *
 *--------------------------------------------------------------
 */

/* Note: this is likely to be incomplete! */

int 
Tk_ConfigStrokePathStyleGC(
        XGCValues *gcValues, Tk_Canvas canvas,
        Tk_Item *item, Tk_PathStyle *style)
{
    int mask = 0;
    double width;
    Tk_Dash *dash;
    XColor *color;
    Pixmap stipple;
    Tk_State state = item->state;

    if (style->strokeWidth < 0.0) {
	style->strokeWidth = 0.0;
    }
    if (state==TK_STATE_HIDDEN) {
	return 0;
    }

    width = style->strokeWidth;
    if (width < 1.0) {
	width = 1.0;
    }
    dash = &(style->dash);
    color = style->strokeColor;
    stipple = style->strokeStipple;
    if (state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (color == NULL) {
        return 0;
    }

    gcValues->line_width = (int) (width + 0.5);
    if (color != NULL) {
        gcValues->foreground = color->pixel;
        mask = GCForeground|GCLineWidth;
        if (stipple != None) {
            gcValues->stipple = stipple;
            gcValues->fill_style = FillStippled;
            mask |= GCStipple|GCFillStyle;
        }
    }
    if (mask && (dash->number != 0)) {
        gcValues->line_style = LineOnOffDash;
        gcValues->dash_offset = style->offset;
        if (dash->number >= 2) {
            gcValues->dashes = 4;
        } else if (dash->number > 0) {
            gcValues->dashes = dash->pattern.array[0];
        } else {
            gcValues->dashes = (char) (4 * width);
        }
        mask |= GCLineStyle|GCDashList|GCDashOffset;
    }
    return mask;
}

int 
Tk_ConfigFillPathStyleGC(XGCValues *gcValues, Tk_Canvas canvas,
        Tk_Item *item, Tk_PathStyle *style)
{
    int mask = 0;
    XColor *color;
    Pixmap stipple;

    color = style->fillColor;
    stipple = style->fillStipple;

    if (color != NULL) {
        gcValues->foreground = color->pixel;
        mask = GCForeground;
        if (stipple != None) {
            gcValues->stipple = stipple;
            gcValues->fill_style = FillStippled;
            mask |= GCStipple|GCFillStyle;
        }
    }
    return mask;
}

/* 
 +++ This starts the canvas item part +++ 
 */

/*
 *--------------------------------------------------------------
 *
 * CreatePath --
 *
 *		This procedure is invoked to create a new line item in
 *		a canvas.
 *
 * Results:
 *		A standard Tcl return value.  If an error occurred in
 *		creating the item, then an error message is left in
 *		the interp's result;  in this case itemPtr is left uninitialized,
 *		so it can be safely freed by the caller.
 *
 * Side effects:
 *		A new line item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreatePath(
        Tcl_Interp *interp, 	/* Used for error reporting. */
        Tk_Canvas canvas, 	/* Canvas containing item. */
        Tk_Item *itemPtr, 	/* Item to create. */
        int objc,		/* Number of elements in objv.  */
        Tcl_Obj *CONST objv[])	/* Arguments describing the item. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }
    gInterp = interp;

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */

    Tk_CreateOutline(&(pathPtr->outline));
    Tk_CreatePathStyle(&(pathPtr->style));
    pathPtr->canvas = canvas;
    pathPtr->pathObjPtr = NULL;
    pathPtr->pathLen = 0;
    pathPtr->normPathObjPtr = NULL;
    pathPtr->styleName = NULL;
    pathPtr->atomPtr = NULL;
    pathPtr->bareBbox = NewEmptyPathRect();
    pathPtr->totalBbox = NewEmptyPathRect();
    pathPtr->maxNumSegments = 0;
    pathPtr->flags = 0L;
    
    /* Forces a computation of the normalized path in PathCoords. */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /*
     * The first argument must be the path definition list.
     */

    if (PathCoords(interp, canvas, itemPtr, 1, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePath(interp, canvas, itemPtr, objc-1, objv+1, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    DeletePath(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * PathCoords --
 *
 *		This procedure is invoked to process the "coords" widget
 *		command on lines.  See the user documentation for details
 *		on what it does.
 *
 * Results:
 *		Returns TCL_OK or TCL_ERROR, and sets the interp's result.
 *
 * Side effects:
 *		The coordinates for the given item may be changed.
 *
 *--------------------------------------------------------------
 */

static int
PathCoords(
    Tcl_Interp *interp,			/* Used for error reporting. */
    Tk_Canvas canvas,			/* Canvas containing item. */
    Tk_Item *itemPtr,			/* Item whose coordinates are to be
					 * read or modified. */
    int objc,				/*  */
    Tcl_Obj *CONST objv[])		/*  */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = NULL;
    int result, len;
    
    if (objc == 0) {
        /* @@@ We have an option here if to return the normalized or original path. */
        //Tcl_SetObjResult(interp, pathPtr->pathObjPtr);
        
        /* We may need to recompute the normalized path from the atoms. */
        if (pathPtr->flags & kPathItemNeedNewNormalizedPath) {
            if (pathPtr->normPathObjPtr != NULL) {
                Tcl_DecrRefCount(pathPtr->normPathObjPtr);
            }
            TkPathNormalize(interp, pathPtr->atomPtr, &(pathPtr->normPathObjPtr));
        }
        Tcl_SetObjResult(interp, pathPtr->normPathObjPtr);
        return TCL_OK;
    } else if (objc == 1) {
        result = TkPathParseToAtoms(interp, objv[0], &atomPtr, &len);
        if (result == TCL_OK) {
        
            /* Free any old atoms. */
            if (pathPtr->atomPtr != NULL) {
                TkPathFreeAtoms(pathPtr->atomPtr);
            }
            pathPtr->atomPtr = atomPtr;
            pathPtr->pathLen = len;
            pathPtr->pathObjPtr = objv[0];
            Tcl_IncrRefCount(pathPtr->pathObjPtr);
            ComputePathBbox(canvas, pathPtr);
            pathPtr->maxNumSegments = GetSubpathMaxNumSegments(atomPtr);
        }
        return result;
    } else {
        Tcl_WrongNumArgs(interp, 0, objv, "pathName coords id ?pathSpec?");
        return TCL_ERROR;
    }
}

static void
IncludePointInRect(PathRect *r, double x, double y)
{
    r->x1 = MIN(r->x1, x);
    r->y1 = MIN(r->y1, y);
    r->x2 = MAX(r->x2, x);
    r->y2 = MAX(r->y2, y);
}

static void
SetTotalBboxFromBare(PathItem *pathPtr)
{
    Tk_PathStyle *style = &(pathPtr->style);
    double width;
    PathRect rect;
    
    rect = pathPtr->bareBbox;

    width = 0.0;
    if (style->strokeColor != NULL) {
        width = style->strokeWidth;
        if (width < 1.0) {
            width = 1.0;
        }
        rect.x1 -= width;
        rect.x2 += width;
        rect.y1 -= width;
        rect.y2 += width;
    }
    
    /* @@@ TODO: We should have a method here to add the necessary space
     * needed for sharp miter line joins.
     */
    
    /*
     * Add one more pixel of fudge factor just to be safe (e.g.
     * X may round differently than we do). Antialiasing?
     */
    rect.x1 -= 1.0;
    rect.x2 += 1.0;
    rect.y1 -= 1.0;
    rect.y2 += 1.0;
    
    pathPtr->totalBbox = rect;
}

/*
 *--------------------------------------------------------------
 *
 * SetPathHeaderBbox --
 *
 *		This procedure sets the (transformed) bbox in the items header.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The fields x1, y1, x2, and y2 are updated in the header
 *		for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
SetPathHeaderBbox(PathItem *pathPtr)
{
    Tk_PathStyle *style = &(pathPtr->style);
    PathRect rect;
    
    rect = pathPtr->totalBbox;

    if (style->matrixPtr != NULL) {
        double x, y;
        PathRect r = NewEmptyPathRect();

        /* Take each four corners in turn. */
        x = rect.x1, y = rect.y1;
        PathApplyTMatrix(style->matrixPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x2, y = rect.y1;
        PathApplyTMatrix(style->matrixPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x1, y = rect.y2;
        PathApplyTMatrix(style->matrixPtr, &x, &y);
        IncludePointInRect(&r, x, y);

        x = rect.x2, y = rect.y2;
        PathApplyTMatrix(style->matrixPtr, &x, &y);
        IncludePointInRect(&r, x, y);
        rect = r;  
    }
    pathPtr->header.x1 = (int) rect.x1;
    pathPtr->header.x2 = (int) rect.x2;
    pathPtr->header.y1 = (int) rect.y1;
    pathPtr->header.y2 = (int) rect.y2;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigurePath --
 *
 *		This procedure is invoked to configure various aspects
 *		of a line item such as its background color.
 *
 * Results:
 *		A standard Tcl result code.  If an error occurs, then
 *		an error message is left in the interp's result.
 *
 * Side effects:
 *		Configuration information, such as colors and stipple
 *		patterns, may be set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigurePath(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_Canvas canvas,		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr,		/* Line item to reconfigure. */
    int objc,			/* Number of elements in objv.  */
    Tcl_Obj *CONST objv[],	/* Arguments describing things to configure. */
    int flags)			/* Flags to pass to Tk_ConfigureWidget. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathStyle *style = &(pathPtr->style);
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    Tk_Window tkwin;
    Tk_State state;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) pathPtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    style->strokeOpacity = MAX(0.0, MIN(1.0, style->strokeOpacity));
    style->fillOpacity   = MAX(0.0, MIN(1.0, style->fillOpacity));
    
    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (pathPtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, style, pathPtr->styleName);
    } 

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    state = itemPtr->state;

    /*
    if (pathPtr->outline.activeWidth > pathPtr->outline.width ||
	    pathPtr->outline.activeDash.number != 0 ||
	    pathPtr->outline.activeColor != NULL ||
	    pathPtr->outline.activeStipple != None) {
	itemPtr->redraw_flags |= TK_ITEM_STATE_DEPENDANT;
    } else {
	itemPtr->redraw_flags &= ~TK_ITEM_STATE_DEPENDANT;
    }
    */
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        //ComputePathBbox(canvas, pathPtr);
        return TCL_OK;
    }
    
    /* @@@ Not sure if GC's should be used at all! */
    
    /* Configure the stroke GC. */
    mask = Tk_ConfigStrokePathStyleGC(&gcValues, canvas, itemPtr,
	    &(pathPtr->style));
    if (mask) {
        gcValues.cap_style = pathPtr->style.capStyle;
        mask |= GCCapStyle;

        gcValues.join_style = pathPtr->style.joinStyle;
        mask |= GCJoinStyle;
        newGC = Tk_GetGC(tkwin, mask, &gcValues);
    } else {
        newGC = None;
    }
    if (pathPtr->style.strokeGC != None) {
        Tk_FreeGC(Tk_Display(tkwin), pathPtr->style.strokeGC);
    }
    pathPtr->style.strokeGC = newGC;

    /* Configure the fill GC. */
    mask = Tk_ConfigFillPathStyleGC(&gcValues, canvas, itemPtr,
            &(pathPtr->style));
    if (mask) {
        newGC = Tk_GetGC(tkwin, mask, &gcValues);
    } else {
        newGC = None;
    }
    if (pathPtr->style.fillGC != None) {
        Tk_FreeGC(Tk_Display(tkwin), pathPtr->style.fillGC);
    }
    pathPtr->style.fillGC = newGC;

    /*
     * Recompute bounding box for path.
     * Do a simplified version here starting from the bare bbox.
     * Note: This requires that bareBbox already computed!
     */
    SetTotalBboxFromBare(pathPtr);
    SetPathHeaderBbox(pathPtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeletePath --
 *
 *		This procedure is called to clean up the data structure
 *		associated with a line item.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Resources associated with itemPtr are released.
 *
 *--------------------------------------------------------------
 */

static void
DeletePath(
    Tk_Canvas canvas,			/* Info about overall canvas widget. */
    Tk_Item *itemPtr,			/* Item that is being deleted. */
    Display *display)			/* Display containing window for
					 * canvas. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;

    Tk_DeleteOutline(display, &(pathPtr->outline));
    if (pathPtr->pathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->pathObjPtr);
    }
    if (pathPtr->normPathObjPtr != NULL) {
        Tcl_DecrRefCount(pathPtr->normPathObjPtr);
    }
    if (pathPtr->atomPtr != NULL) {
	TkPathFreeAtoms(pathPtr->atomPtr);
        pathPtr->atomPtr = NULL;
    }
    if (pathPtr->styleName != NULL) {
        ckfree(pathPtr->styleName);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetBareArcBbox
 *
 *		Gets an overestimate of the bounding box rectangle of
 * 		an arc defined using central parametrization assuming
 *		zero stroke width.
 * 		Untransformed coordinates!
 *		Note: 1) all angles clockwise direction!
 *	    	  2) all angles in radians.
 *
 * Results:
 *		A PathRect.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static PathRect
GetBareArcBbox(double cx, double cy, double rx, double ry,
        double theta1, double dtheta, double phi)
{
    PathRect r = {1.0e36, 1.0e36, -1.0e36, -1.0e36};	/* Empty rect. */
    double start, extent, stop, stop2PI;
    double cosStart, sinStart, cosStop, sinStop;
    
    /* Keep 0 <= start, extent < 2pi 
     * and 0 <= stop < 4pi */
    if (dtheta >= 0.0) {
        start = theta1;
        extent = dtheta;
    } else {
        start = theta1 + dtheta;
        extent = -1.0*dtheta;
    }
    if (start < 0.0) {
        start += 2.0*PI;
        if (start < 0.0) {
            start += 2.0*PI;
        }
    }
    if (start >= 2.0*PI) {
        start -= 2.0*PI;
    }
    stop = start + extent;
    stop2PI = stop - 2.0*PI;
    cosStart = cos(start);
    sinStart = sin(start);
    cosStop = cos(stop);
    sinStop = sin(stop);
    
    /*
     * Compute bbox for phi = 0.
     * Put everything at (0,0) and shift to (cx,cy) at the end.
     * Look for extreme points of arc:
     * 	1) start and stop points
     *	2) any intersections of x and y axes
     * Count both first and second "turns".
     */
                
    IncludePointInRect(&r, rx*cosStart, ry*sinStart);
    IncludePointInRect(&r, rx*cosStop,  ry*sinStop);
    if (((start < PI/2.0) && (stop > PI/2.0)) || (stop2PI > PI/2.0)) {
        IncludePointInRect(&r, 0.0, ry);
    }
    if (((start < PI) && (stop > PI)) || (stop2PI > PI)) {
        IncludePointInRect(&r, -rx, 0.0);
    }
    if (((start < 3.0*PI/2.0) && (stop > 3.0*PI/2.0)) || (stop2PI > 3.0*PI/2.0)) {
        IncludePointInRect(&r, 0.0, -ry);
    }
    if (stop > 2.0*PI) {
        IncludePointInRect(&r, rx, 0.0);
    }
    
    /*
     * Rotate the bbox above to get an overestimate of extremas.
     */
    if (fabs(phi) > 1e-6) {
        double cosPhi, sinPhi;
        double x, y;
        PathRect rrot = {1.0e36, 1.0e36, -1.0e36, -1.0e36};
        
        cosPhi = cos(phi);
        sinPhi = sin(phi);
        x = r.x1*cosPhi - r.y1*sinPhi;
        y = r.x1*sinPhi + r.y1*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x2*cosPhi - r.y1*sinPhi;
        y = r.x2*sinPhi + r.y1*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x1*cosPhi - r.y2*sinPhi;
        y = r.x1*sinPhi + r.y2*cosPhi;
        IncludePointInRect(&rrot, x, y);
        
        x = r.x2*cosPhi - r.y2*sinPhi;
        y = r.x2*sinPhi + r.y2*cosPhi;
        IncludePointInRect(&rrot, x, y);

        r = rrot;
    }
    
    /* Shift rect to arc center. */
    r.x1 += cx;
    r.y1 += cy;
    r.x2 += cx;
    r.y2 += cy;
    return r;
}

/*
 *--------------------------------------------------------------
 *
 * GetBarePathBbox
 *
 *		Gets an overestimate of the bounding box rectangle of
 * 		a path assuming zero stroke width.
 * 		Untransformed coordinates!
 *
 * Results:
 *		A PathRect.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static PathRect
GetBarePathBbox(PathAtom *atomPtr)
{
    double x1, y1, x2, y2, x3, y3, x4, y4, x5, y5;
    double currentX, currentY;
    PathRect r = {1.0e36, 1.0e36, -1.0e36, -1.0e36};

    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: { 
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                IncludePointInRect(&r, move->x, move->y);
                currentX = move->x;
                currentY = move->y;
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;

                IncludePointInRect(&r, line->x, line->y);
                currentX = line->x;
                currentY = line->y;
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                int result;
                double cx, cy, rx, ry;
                double theta1, dtheta;
            
                result = EndpointToCentralArcParameters(
                        currentX, currentY,
                        arc->x, arc->y, arc->radX, arc->radY, 
                        DEGREES_TO_RADIANS * arc->angle, 
                        arc->largeArcFlag, arc->sweepFlag,
                        &cx, &cy, &rx, &ry,
                        &theta1, &dtheta);
                if (result == kPathArcLine) {
                    IncludePointInRect(&r, arc->x, arc->y);
                } else if (result == kPathArcOK) {
                    PathRect arcRect;
                    
                    arcRect = GetBareArcBbox(cx, cy, rx, ry, theta1, dtheta, 
                            DEGREES_TO_RADIANS * arc->angle);
                    IncludePointInRect(&r, arcRect.x1, arcRect.y1);
                    IncludePointInRect(&r, arcRect.x2, arcRect.y2);
                }
                currentX = arc->x;
                currentY = arc->y;
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                x1 = (currentX + quad->ctrlX)/2.0;
                y1 = (currentY + quad->ctrlY)/2.0;
                x2 = (quad->ctrlX + quad->anchorX)/2.0;
                y2 = (quad->ctrlY + quad->anchorY)/2.0;
                IncludePointInRect(&r, x1, y1);
                IncludePointInRect(&r, x2, y2);
                currentX = quad->anchorX;
                currentY = quad->anchorY;
                IncludePointInRect(&r, currentX, currentY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                x1 = (currentX + curve->ctrlX1)/2.0;
                y1 = (currentY + curve->ctrlY1)/2.0;
                x2 = (curve->ctrlX1 + curve->ctrlX2)/2.0;
                y2 = (curve->ctrlY1 + curve->ctrlY2)/2.0;
                x3 = (curve->ctrlX2 + curve->anchorX)/2.0;
                y3 = (curve->ctrlY2 + curve->anchorY)/2.0;
                IncludePointInRect(&r, x1, y1);
                IncludePointInRect(&r, x3, y3);
                x4 = (x1 + x2)/2.0;
                y4 = (y1 + y2)/2.0;
                x5 = (x2 + x3)/2.0;
                y5 = (y2 + y3)/2.0;
                IncludePointInRect(&r, x4, y4);
                IncludePointInRect(&r, x5, y5);
                currentX = curve->anchorX;
                currentY = curve->anchorY;
                IncludePointInRect(&r, currentX, currentY);
                break;
            }
            case PATH_ATOM_Z: {
                /* empty */
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    return r;
}

/*
 *--------------------------------------------------------------
 *
 * ComputePathBbox --
 *
 *		This procedure is invoked to compute the bounding box of
 *		all the pixels that may be drawn as part of a path.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The fields x1, y1, x2, and y2 are updated in the header
 *		for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputePathBbox(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    PathItem *pathPtr)			/* Item whose bbox is to be
					 * recomputed. */
{
    Tk_State state = pathPtr->header.state;
    PathRect rect;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (pathPtr->pathObjPtr == NULL || (pathPtr->pathLen < 4) || (state == TK_STATE_HIDDEN)) {
        pathPtr->header.x1 = pathPtr->header.x2 =
        pathPtr->header.y1 = pathPtr->header.y2 = -1;
        return;
    }
    
    /*
     * Get an approximation of the path's bounding box
     * assuming zero width outline (stroke).
     */
    rect = GetBarePathBbox(pathPtr->atomPtr);
    pathPtr->bareBbox = rect;

    SetTotalBboxFromBare(pathPtr);
    SetPathHeaderBbox(pathPtr);
}

static void
PaintCanvasLinearGradient(Tk_Canvas canvas, Drawable drawable, PathRect *bbox, char *name, int fillRule)
{
    short drawableX, drawableY;
    PathRect drawableBbox;
    
    Tk_CanvasDrawableCoords(canvas, bbox->x1, bbox->y1, &drawableX, &drawableY); 
    drawableBbox.x1 = drawableX;
    drawableBbox.y1 = drawableY;
    Tk_CanvasDrawableCoords(canvas, bbox->x2, bbox->y2, &drawableX, &drawableY); 
    drawableBbox.x2 = drawableX;
    drawableBbox.y2 = drawableY;
    PathPaintLinearGradientFromName(drawable, &drawableBbox, name, fillRule);
}

/*
 *--------------------------------------------------------------
 *
 * DisplayPath --
 *
 *		This procedure is invoked to draw a line item in a given
 *		drawable.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		ItemPtr is drawn in drawable using the transformation
 *		information in canvas.
 *
 *--------------------------------------------------------------
 */

static void
DisplayPath(
    Tk_Canvas canvas,			/* Canvas that contains item. */
    Tk_Item *itemPtr,			/* Item to be displayed. */
    Display *display,			/* Display on which to draw item. */
    Drawable drawable,			/* Pixmap or window in which to draw
					 * item. */
    int x, int y, 			/* Describes region of canvas that */
    int width, int height)	 	/* must be redisplayed (not used). */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pathPtr->style);
    
    /*
     * Define the path in the drawable using the path drawing functions.
     */
     
    TkPathInit(drawable);
    if (MakeCanvasPath(canvas, pathPtr, drawable) != TCL_OK) {
        return;
    }
    
    /*
     * And do the necessary paintjob. 
     * What if both -fill and -filllineargradient?
     */     
    if (stylePtr->gradientFillName != NULL) {
        if (HaveLinearGradientStyleWithName(stylePtr->gradientFillName) == TCL_OK) {
            TkPathClipToPath(drawable, stylePtr->fillRule);
            PaintCanvasLinearGradient(canvas, drawable, &(pathPtr->bareBbox), 
                    stylePtr->gradientFillName, stylePtr->fillRule);

            /* Note: Both CoreGraphics on MacOSX and Win32 GDI clears the current path
             *       when setting clipping. Need therefore to redo the path. 
             */
            if (TkPathDrawingDestroysPath()) {
                MakeCanvasPath(canvas, pathPtr, drawable);
            }
            
            /* We shall remove the path clipping here! */
            TkPathReleaseClipToPath(drawable);
        }
    }
     
    if ((stylePtr->fillColor != NULL) && (stylePtr->strokeColor != NULL)) {
        TkPathFillAndStroke(drawable, stylePtr);
    } else if (stylePtr->fillColor != NULL) {
        TkPathFill(drawable, stylePtr);
    } else if (stylePtr->strokeColor != NULL) {
        TkPathStroke(drawable, stylePtr);
    }
    TkPathFree(drawable);
}

/*
 *--------------------------------------------------------------
 *
 * PathToPoint --
 *
 *		Computes the distance from a given point to a given
 *		line, in canvas units.
 *
 * Results:
 *		The return value is 0 if the point whose x and y coordinates
 *		are pointPtr[0] and pointPtr[1] is inside the line.  If the
 *		point isn't inside the line then the return value is the
 *		distance from the point to the line.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static double
PathToPoint(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against point. */
    double *pointPtr)		/* Pointer to x and y coordinates. */
{
    Tk_State 		state = itemPtr->state;
    int				numPoints, numStrokes;
    int				isclosed;
    int				intersections, nonzerorule;
    int				sumIntersections = 0, sumNonzerorule = 0;
    double 			*polyPtr;
    double 			bestDist, radius, width, dist;
    PathItem 		*pathPtr = (PathItem *) itemPtr;
    PathAtom		*atomPtr = pathPtr->atomPtr;
    Tk_PathStyle 	*stylePtr = &(pathPtr->style);
    TMatrix 		*matrixPtr = stylePtr->matrixPtr;

    bestDist = 1.0e36;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if ((pathPtr->pathLen <= 3) || itemPtr->state==TK_STATE_HIDDEN) {
        return bestDist;
    }
    if ((stylePtr->fillColor == NULL) && (stylePtr->strokeColor == NULL)) {
        return bestDist;
    }
    
    /* 
     * Do we need more memory or can we use static space? 
     */
    if (pathPtr->maxNumSegments > MAX_NUM_STATIC_SEGMENTS) {
        polyPtr = (double *) ckalloc((unsigned) (2*pathPtr->maxNumSegments*sizeof(double)));
    } else {
        polyPtr = staticSpace;
    }
    width = stylePtr->strokeWidth;
    if (width < 1.0) {
        width = 1.0;
    }
    radius = width/2.0;

    /*
     * Loop through each subpath, creating the approximate polyline,
     * and do the *ToPoint functions.
     *
     * Note: Strokes can be treated independently for each subpath,
     *		 but fills cannot since subpaths may intersect creating
     *		 "holes".
     */
     
#if PATH_DEBUG
    DebugPrintf(gInterp, 2, "PathToPoint..........");
#endif
    
    while (atomPtr != NULL) {
        MakeSubPathSegments(&atomPtr, polyPtr, &numPoints, &numStrokes, matrixPtr);
        isclosed = 0;
        if (numStrokes == numPoints) {
            isclosed = 1;
        }        
#if PATH_DEBUG
        {
            int i;
            
            DebugPrintf(gInterp, 2, "numPoints=%d, isclosed=%d, atomPtr=0x%.8x", numPoints, isclosed, atomPtr);
            for (i = 0; i < numPoints; i++) {
                DebugPrintf(gInterp, 2, "\t %6.1f, %6.1f", polyPtr[2*i], polyPtr[2*i+1]);
            }
        }
#endif        
        /*
         * This gives the min distance to the *stroke* AND the
         * number of intersections of the two types.
         */
        dist = PathPolygonToPointEx(polyPtr, numPoints, pointPtr, 
                &intersections, &nonzerorule);
        sumIntersections += intersections;
        sumNonzerorule += nonzerorule;
        if (stylePtr->strokeColor != NULL) {
        
            /*
             * This gives the distance to a zero width polyline.
             * Use a simple scheme to adjust for a small width.
             */
            dist -= radius;
        }
        if (dist < bestDist) {
            bestDist = dist;
        }
        if (bestDist <= 0.0) {
            bestDist = 0.0;
            goto done;
        }

        /*
         * For wider strokes we must make a more detailed analysis.
         */
        if ((stylePtr->strokeColor != NULL) && (stylePtr->strokeWidth > kStrokeThicknessLimit)) {
            dist = PathThickPolygonToPoint(stylePtr->joinStyle, stylePtr->capStyle, 
                    width, isclosed, polyPtr, numPoints, pointPtr);
            if (dist < bestDist) {
                bestDist = dist;
            }
            if (bestDist <= 0.0) {
                bestDist = 0.0;
                goto done;
            }
        }
    }        

    /*
     * We've processed all of the points.  
     * EvenOddRule: If the number of intersections is odd, 
     *			the point is inside the polygon.
     * WindingRule (nonzero): If the number of directed intersections
     *			are nonzero, then inside.
     */
    if (stylePtr->fillColor != NULL) {
        if ((stylePtr->fillRule == EvenOddRule) && (sumIntersections & 0x1)) {
            bestDist = 0.0;
        } else if ((stylePtr->fillRule == WindingRule) && (sumNonzerorule != 0)) {
            bestDist = 0.0;
        }
    }
    
done:
    if (polyPtr != staticSpace) {
        ckfree((char *) polyPtr);
    }
    return bestDist;
}

/**********************************/

double
TkLineToPoint2(end1Ptr, end2Ptr, pointPtr)
    double end1Ptr[2];		/* Coordinates of first end-point of line. */
    double end2Ptr[2];		/* Coordinates of second end-point of line. */
    double pointPtr[2];		/* Points to coords for point. */
{
    double dx, dy, a2, b2, c2;

    /*
     * Compute the point on the line that is closest to the
     * point. Use Pythagoras!
     * Notation:
     *	a = distance between end1 and end2
     * 	b = distance between end1 and point
     *	c = distance between end2 and point
     *
     *   point
     *    |\
     *    | \
     *  b |  \ c
     *    |   \
     *    |----\
     * end1  a  end2
     *
     * If angle between a and b is 90 degrees: c2 = a2 + b2
     * If larger then c2 > a2 + b2 and end1 is closest to point
     * Similar for end2 with b and c interchanged.
     */
     
    dx = end1Ptr[0] - end2Ptr[0];
    dy = end1Ptr[1] - end2Ptr[1];
    a2 = dx*dx + dy*dy;

    dx = end1Ptr[0] - pointPtr[0];
    dy = end1Ptr[1] - pointPtr[1];
    b2 = dx*dx + dy*dy;

    dx = end2Ptr[0] - pointPtr[0];
    dy = end2Ptr[1] - pointPtr[1];
    c2 = dx*dx + dy*dy;
    
    if (c2 >= a2 + b2) {
        return sqrt(b2);
    } else if (b2 >= a2 + c2) {
        return sqrt(c2);
    } else {
        double delta;
        
        /* 
         * The closest point is found at the point between end1 and end2
         * that is perp to point. delta is the distance from end1 along
         * that line which is closest to point.
         */
        delta = (a2 + b2 - c2)/(2.0*sqrt(a2));
        return sqrt(MAX(0.0, b2 - delta*delta));
    }
}

/* 
 * Get maximum number of segments needed to describe path. 
 * Needed to see if we can use static space or need to allocate more.
 */

static int
GetArcNumSegments(double currentX, double currentY, ArcAtom *arc)
{
    int result;
    int ntheta, nlength;
    int numSteps;			/* Number of curve points to
					 * generate.  */
    double cx, cy, rx, ry;
    double theta1, dtheta;

    result = EndpointToCentralArcParameters(
            currentX, currentY,
            arc->x, arc->y, arc->radX, arc->radY, 
            DEGREES_TO_RADIANS * arc->angle, 
            arc->largeArcFlag, arc->sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
    if (result == kPathArcLine) {
        return 2;
    } else if (result == kPathArcSkip) {
        return 0;
    }

    /* Estimate the number of steps needed. 
     * Max 10 degrees or length 50.
     */
    ntheta = (int) (dtheta/5.0 + 0.5);
    nlength = (int) (0.5*(rx + ry)*dtheta/50 + 0.5);
    numSteps = MAX(4, MAX(ntheta, nlength));;
    return numSteps;
}

static int
GetSubpathMaxNumSegments(PathAtom *atomPtr)
{
    int			num = 0;
    int 		maxNumSegments = 0;
    int 		first = 1;
    double 		currentX = 0.0, currentY = 0.0;
    double 		startX = 0.0, startY = 0.0;
    MoveToAtom 	*move;
    LineToAtom 	*line;
    ArcAtom 	*arc;
    QuadBezierAtom *quad;
    CurveToAtom *curve;
    
    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
                if (first) {
                    num = 0;
                } else {
                    if (num > maxNumSegments) {
                        maxNumSegments = num;
                    }
                }
                num++;
                currentX = move->x;
                currentY = move->y;
                startX = currentX;
                startY = currentY;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                num++;
                currentX = line->x;
                currentY = line->y;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                num += GetArcNumSegments(currentX, currentY, arc);
                currentX = arc->x;
                currentY = arc->y;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                num += kNumSegmentsQuadBezier;
                currentX = quad->anchorX;
                currentY = quad->anchorY;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                num += kNumSegmentsCurveTo;
                currentX = curve->anchorX;
                currentY = curve->anchorY;
                break;
            }
            case PATH_ATOM_Z: {
                num++;
                currentX = startX;
                currentY = startY;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    return maxNumSegments;
}

/*
 *--------------------------------------------------------------
 *
 * ArcSegments --
 *
 *		Given the arc parameters it makes a sequence if line segments.
 *		All angles in radians!
 *		Note that segments are transformed!
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the arc.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static void
ArcSegments(
    CentralArcPars *arcPars,
    TMatrix *matrixPtr,
    int includeFirst,			/* Should the first point be included? */
    int numSteps,			/* Number of curve segments to
					 * generate.  */
    register double *coordPtr)		/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double cosPhi, sinPhi;
    double cosAlpha, sinAlpha;
    double alpha, dalpha, theta1;
    double cx, cy, rx, ry;
    
    cosPhi = cos(arcPars->phi);
    sinPhi = sin(arcPars->phi);
    cx = arcPars->cx;
    cy = arcPars->cy;
    rx = arcPars->rx;
    ry = arcPars->ry;
    theta1 = arcPars->theta1;
    dalpha = arcPars->dtheta/numSteps;

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        alpha = theta1 + i*dalpha;
        cosAlpha = cos(alpha);
        sinAlpha = sin(alpha);
        coordPtr[0] = cx + rx*cosAlpha*cosPhi - ry*sinAlpha*sinPhi;
        coordPtr[1] = cy + rx*cosAlpha*sinPhi + ry*sinAlpha*cosPhi;
        PathApplyTMatrix(matrixPtr, coordPtr, coordPtr+1);
    }
}

/*
 *--------------------------------------------------------------
 *
 * CurveSegments --
 *
 *		Given four control points, create a larger set of points
 *		for a cubic Bezier spline based on the points.
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the Bezier spline defined
 *		by the four control points.  
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static void
CurveSegments(
    double control[],		/* Array of coordinates for four
                             * control points:  x0, y0, x1, y1,
                             * ... x3 y3. */
    int includeFirst,		/* Should the first point be included? */
    int numSteps,			/* Number of curve segments to
                             * generate.  */
    register double *coordPtr)		/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double u, u2, u3, t, t2, t3;

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        t = ((double) i)/((double) numSteps);
        t2 = t*t;
        t3 = t2*t;
        u = 1.0 - t;
        u2 = u*u;
        u3 = u2*u;
        coordPtr[0] = control[0]*u3
                + 3.0 * (control[2]*t*u2 + control[4]*t2*u) + control[6]*t3;
        coordPtr[1] = control[1]*u3
                + 3.0 * (control[3]*t*u2 + control[5]*t2*u) + control[7]*t3;
    }
}

/*
 *--------------------------------------------------------------
 *
 * QuadBezierSegments --
 *
 *		Given three control points, create a larger set of points
 *		for a quadratic Bezier spline based on the points.
 *
 * Results:
 *		The array at *coordPtr gets filled in with 2*numSteps
 *		coordinates, which correspond to the quadratic Bezier spline defined
 *		by the control points.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static void
QuadBezierSegments(
    double control[],			/* Array of coordinates for three
                                 * control points:  x0, y0, x1, y1,
                                 * x2, y2. */
    int includeFirst,			/* Should the first point be included? */
    int numSteps,				/* Number of curve segments to
                                 * generate.  */
    register double *coordPtr)	/* Where to put new points. */
{
    int i;
    int istart = 1 - includeFirst;
    double u, u2, t, t2;

#if PATH_DEBUG
    DebugPrintf(gInterp, 2, "QuadBezierSegments %6.0f, %6.0f, %6.0f, %6.0f, %6.0f, %6.0f", 
            control[0], control[1], control[2], control[3], control[4], control[5]);
#endif

    for (i = istart; i <= numSteps; i++, coordPtr += 2) {
        t = ((double) i)/((double) numSteps);
        t2 = t*t;
        u = 1.0 - t;
        u2 = u*u;
        coordPtr[0] = control[0]*u2 + 2.0 * control[2]*t*u + control[4]*t2;
        coordPtr[1] = control[1]*u2 + 2.0 * control[3]*t*u + control[5]*t2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * AddArcSegments, AddQuadBezierSegments, AddCurveToSegments --
 *
 *		Adds a number of points along the arc (curve) to coordPtr
 *		representing straight line segments.
 *
 * Results:
 *		Number of points added. 
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
AddArcSegments(
    TMatrix *matrixPtr,
    double current[2],		/* Current point. */
    ArcAtom *arc,
    double *coordPtr)		/* Where to put the points. */
{
    int result;
    int numPoints;
    CentralArcPars arcPars;
    double cx, cy, rx, ry;
    double theta1, dtheta;
            
    /*
     * Note: The arc parametrization used cannot generally
     * be transformed. Need to transform each line segment separately!
     */
    
    result = EndpointToCentralArcParameters(
            current[0], current[1],
            arc->x, arc->y, arc->radX, arc->radY, 
            DEGREES_TO_RADIANS * arc->angle, 
            arc->largeArcFlag, arc->sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
    if (result == kPathArcLine) {
        double pts[2];

        pts[0] = arc->x;
        pts[1] = arc->y;
        PathApplyTMatrix(matrixPtr, pts, pts+1);
        coordPtr[0] = pts[0];
        coordPtr[1] = pts[1];
        return 1;
    } else if (result == kPathArcSkip) {
        return 0;
    }

    arcPars.cx = cx;
    arcPars.cy = cy;
    arcPars.rx = rx;
    arcPars.ry = ry;
    arcPars.theta1 = theta1;
    arcPars.dtheta = dtheta;
    arcPars.phi = arc->angle;

    numPoints = GetArcNumSegments(current[0], current[1], arc);    
    ArcSegments(&arcPars, matrixPtr, 0, numPoints, coordPtr);

    return numPoints;
}

static int
AddQuadBezierSegments(
    TMatrix *matrixPtr,
    double current[2],		/* Current point. */
    QuadBezierAtom *quad,
    double *coordPtr)		/* Where to put the points. */
{
    int numPoints;			/* Number of curve points to
                             * generate.  */
    double control[6];

    PathApplyTMatrixToPoint(matrixPtr, current, control);
    PathApplyTMatrixToPoint(matrixPtr, &(quad->ctrlX), control+2);
    PathApplyTMatrixToPoint(matrixPtr, &(quad->anchorX), control+4);

    numPoints = kNumSegmentsQuadBezier;
    QuadBezierSegments(control, 0, numPoints, coordPtr);
    
#if PATH_DEBUG
        {
            int i;
            
            DebugPrintf(gInterp, 2, "AddQuadBezierSegments: numPoints=%d", numPoints);
            for (i = 0; i < numPoints; i++) {
                DebugPrintf(gInterp, 2, "\t %6.1f, %6.1f", coordPtr[2*i], coordPtr[2*i+1]);
            }
        }
#endif

    return numPoints;
}

static int
AddCurveToSegments(
    TMatrix *matrixPtr,
    double current[2],			/* Current point. */
    CurveToAtom *curve,
    double *coordPtr)
{
    int numSteps;				/* Number of curve points to
                                 * generate.  */
    double control[8];

    PathApplyTMatrixToPoint(matrixPtr, current, control);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->ctrlX1), control+2);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->ctrlX2), control+4);
    PathApplyTMatrixToPoint(matrixPtr, &(curve->anchorX), control+6);

    numSteps = kNumSegmentsCurveTo;
    CurveSegments(control, 1, numSteps, coordPtr);
    
    return numSteps;
}

/*
 *--------------------------------------------------------------
 *
 * SubPathToArea --
 *
 *		This procedure is called to determine whether a subpath
 *		lies entirely inside, entirely outside, or overlapping
 *		a given rectangular area.
 *
 * Results:
 *		-1 is returned if the item is entirely outside the
 *		area, 0 if it overlaps, and 1 if it is entirely
 *		inside the given area.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
SubPathToArea(
    PathItem 	*pathPtr, 
    double 		*polyPtr, 
    int 		numPoints, 		/* Total number of points. First one
                                 * is duplicated in the last. */
    int			numStrokes,		/* The number of strokes which is one less
                                 * than numPoints if path not closed. */
    double 		*rectPtr, 
    int 		inside)			/* This is the current inside status. */
{
    Tk_PathStyle *stylePtr = &(pathPtr->style);
    double width;
    
    /* @@@ 	There is an open question how a closed unfilled polygon
     *		completely enclosing the area rect should be counted.
     *		While the tk canvas polygon item counts it as intersecting (0),
     *		the line item counts it as outside (-1).
     */
    
    if (stylePtr->fillColor != NULL) {
    
        /* This checks a closed polygon with zero width for inside.
         * If area rect completely enclosed it returns intersecting (0).
         */
        inside = TkPolygonToArea(polyPtr, numPoints, rectPtr);
        if (inside = 0) {
            return 0;
        }
    }
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
        if (width < 1.0) {
            width = 1.0;
        }
        if (stylePtr->strokeWidth > kStrokeThicknessLimit) {
            inside = TkThickPolyLineToArea(polyPtr, numStrokes, 
                    width, stylePtr->capStyle, 
                    stylePtr->joinStyle, rectPtr);
        } else {
			inside = PathPolyLineToArea(polyPtr, numStrokes, rectPtr);        
        }
    }
    return inside;
}

/*
 *--------------------------------------------------------------
 *
 * MakeSubPathSegments --
 *
 *		Supposed to be a generic segment generator that can be used 
 *		by both Area and Point functions.
 *
 * Results:
 *		Points filled into polyPtr...
 *
 * Side effects:
 *		Pointer *atomPtrPtr may be updated.
 *
 *--------------------------------------------------------------
 */

static void
MakeSubPathSegments(PathAtom **atomPtrPtr, double *polyPtr, 
        int *numPointsPtr, int *numStrokesPtr, TMatrix *matrixPtr)
{
    int 			first = 1;
    int				numPoints;
    int				numStrokes;
    int				numAdded;
    int				isclosed = 0;
    double 			current[2];		/* Current untransformed point. */
    double			*currentTPtr;	/* Pointer to the transformed current point. */
    double			*coordPtr;
    PathAtom 		*atomPtr;
    MoveToAtom 		*move;
    LineToAtom 		*line;
    ArcAtom 		*arc;
    QuadBezierAtom 	*quad;
    CurveToAtom 	*curve;
    CloseAtom		*close;
    
    /* @@@ 	Note that for unfilled paths we could have made a progressive
     *     	area (point) check which may be faster since we may stop when 0 (overlapping).
     *	   	For filled paths we cannot rely on this since the area rectangle
     *		may be entirely enclosed in the path and still overlapping.
     *		(Need better explanation!)
     */
    
    /*
     * Check each segment of the path.
     * Any transform matrix is applied at the last stage when comparing to rect.
     * 'current' is always untransformed coords.
     */

    current[0] = 0.0;
    current[1] = 0.0;
    numPoints = 0;
    numStrokes = 0;
    isclosed = 0;
    atomPtr = *atomPtrPtr;
    
    while (atomPtr != NULL) {

#if PATH_DEBUG    
        DebugPrintf(gInterp, 2, "atomPtr->type %c", atomPtr->type);
#endif

        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
            
                /* A 'M' atom must be first, may show up later as well. */
                
                if (first) {
                    coordPtr = polyPtr;
                    current[0] = move->x;
                    current[1] = move->y;
                    PathApplyTMatrixToPoint(matrixPtr, current, coordPtr);
                    currentTPtr = coordPtr;
                    coordPtr += 2;
                    numPoints = 1;
                } else {
                
                    /*  
                     * We have finalized a subpath.
                     */
                    goto done;
                }
                first = 0;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                PathApplyTMatrixToPoint(matrixPtr, &(line->x), coordPtr);
                current[0] = line->x;
                current[1] = line->y;
                currentTPtr = coordPtr;
                coordPtr += 2;
                numPoints++;;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                numAdded = AddArcSegments(matrixPtr, current, arc, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = arc->x;
                current[1] = arc->y;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                numAdded = AddQuadBezierSegments(matrixPtr, current,
                        quad, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = quad->anchorX;
                current[1] = quad->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                numAdded = AddCurveToSegments(matrixPtr, current,
                        curve, coordPtr);
                coordPtr += 2 * numAdded;
                numPoints += numAdded;
                current[0] = curve->anchorX;
                current[1] = curve->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Z: {
            
                /* Just add the first point to the end. */
                close = (CloseAtom *) atomPtr;
                coordPtr[0] = polyPtr[0];
                coordPtr[1] = polyPtr[1];
                coordPtr += 2;
                numPoints++;
                current[0]  = close->x;
                current[1]  = close->y;
                isclosed = 1;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }

done:
    if (numPoints > 1) {
        if (isclosed) {
            numStrokes = numPoints;
        } else {
            numStrokes = numPoints - 1;
        }
    }
    *numPointsPtr = numPoints;
    *numStrokesPtr = numStrokes;
    *atomPtrPtr = atomPtr;

    return;
}

/*
 *--------------------------------------------------------------
 *
 * PathToArea --
 *
 *		This procedure is called to determine whether an item
 *		lies entirely inside, entirely outside, or overlapping
 *		a given rectangular area.
 *	
 *		Each subpath is treated in turn. Generate straight line
 *		segments for each subpath and treat it as a polygon.
 *
 * Results:
 *		-1 is returned if the item is entirely outside the
 *		area, 0 if it overlaps, and 1 if it is entirely
 *		inside the given area.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
PathToArea(
    Tk_Canvas canvas,		/* Canvas containing item. */
    Tk_Item *itemPtr,		/* Item to check against line. */
    double *rectPtr)
{
    PathItem 		*pathPtr = (PathItem *) itemPtr;
    int inside;					/* Tentative guess about what to return,
                                 * based on all points seen so far:  one
                                 * means everything seen so far was
                                 * inside the area;  -1 means everything
                                 * was outside the area.  0 means overlap
                                 * has been found. */ 
    int 			first = 1;
    int				numPoints = 0;
    int				numStrokes = 0;
    int				isclosed = 0;
    double 			current[2];		/* Current untransformed point. */
    double 			pt[2];			/* Transformed point. */
    double			*coordPtr, *polyPtr;
    double			*currentTPtr;	/* Pointer to the transformed current point. */
    Tk_State 		state = itemPtr->state;
    Tk_PathStyle 	*stylePtr = &(pathPtr->style);
    PathAtom 		*atomPtr = pathPtr->atomPtr;
    MoveToAtom 		*move;
    LineToAtom 		*line;
    ArcAtom 		*arc;
    QuadBezierAtom 	*quad;
    CurveToAtom 	*curve;
    CloseAtom		*close;
    TMatrix 		*matrixPtr = stylePtr->matrixPtr;
    
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }    
    if ((state==TK_STATE_HIDDEN) || (pathPtr->pathLen < 3)) {
        return -1;
    }

    /* 
     * Do we need more memory or can we use static space? 
     */
    if (pathPtr->maxNumSegments > MAX_NUM_STATIC_SEGMENTS) {
        polyPtr = (double *) ckalloc((unsigned) (2*pathPtr->maxNumSegments*sizeof(double)));
    } else {
        polyPtr = staticSpace;
    }
    coordPtr = polyPtr;
    
    /* @@@ 	Note that for unfilled paths we could have made a progressive
     *     	area check which may be faster since we may stop when 0 (overlapping).
     *	   	For filled paths we cannot rely on this since the area rectangle
     *		may be entirely enclosed in the path and still overlapping.
     *		(Need better explanation!)
     */
    
    /*
     * Check each segment of the path.
     * Any transform matrix is applied at the last stage when comparing to rect.
     * 'current' is always untransformed coords.
     */

    current[0] = 0.0;
    current[1] = 0.0;
    
    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                move = (MoveToAtom *) atomPtr;
            
                /* A 'M' atom must be first, may show up later as well. */

                coordPtr = polyPtr;
                PathApplyTMatrixToPoint(matrixPtr, current, coordPtr);
                current[0] = move->x;
                current[1] = move->y;
                currentTPtr = coordPtr;
                coordPtr += 2;
                numPoints = 1;
                
                if (first) {
                    
                    /*
                     * This defines the starting point. It is either -1 or 1. 
                     * If any subseqent segment has a different 'inside'
                     * then return 0 since one port (in|out)side and another
                     * (out|in)side
                     */
                    inside = -1;
                    if ((currentTPtr[0] >= rectPtr[0]) && (currentTPtr[0] <= rectPtr[2])
                            && (currentTPtr[1] >= rectPtr[1]) && (currentTPtr[1] <= rectPtr[3])) {
                        inside = 1;
                    }
                } else {
                
                    /*  
                     * We have finalized a subpath. Do Area on it.
                     * Need to close it for fill testing.
                     */
                    if (isclosed) {
                        numStrokes = numPoints - 1;
                    } else {
                        numStrokes = numPoints;
                    }
                    inside = SubPathToArea(pathPtr, polyPtr, numPoints, numStrokes, rectPtr, inside);
                    if (inside == 0) {
                        goto done;
                    }
                }
                first = 0;
                isclosed = 0;
                break;
            }
            case PATH_ATOM_L: {
                line = (LineToAtom *) atomPtr;
                PathApplyTMatrixToPoint(matrixPtr, &(line->x), coordPtr);
                current[0] = line->x;
                current[1] = line->y;
                currentTPtr = coordPtr;
                coordPtr += 2;
                numPoints++;;
                break;
            }
            case PATH_ATOM_A: {
                arc = (ArcAtom *) atomPtr;
                numPoints += AddArcSegments(matrixPtr, current, arc, coordPtr);
                coordPtr += 2 * numPoints;
                current[0] = arc->x;
                current[1] = arc->y;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Q: {
                quad = (QuadBezierAtom *) atomPtr;
                numPoints += AddQuadBezierSegments(matrixPtr, current, quad, coordPtr);
                coordPtr += 2 * numPoints;
                current[0] = quad->anchorX;
                current[1] = quad->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_C: {
                curve = (CurveToAtom *) atomPtr;
                numPoints += AddCurveToSegments(matrixPtr, current, curve, coordPtr);
                coordPtr += 2 * numPoints;
                current[0] = curve->anchorX;
                current[1] = curve->anchorY;
                currentTPtr = coordPtr;
                break;
            }
            case PATH_ATOM_Z: {
            
                /* Just add the first point to the end. */
                close = (CloseAtom *) atomPtr;
                coordPtr[0] = polyPtr[0];
                coordPtr[1] = polyPtr[1];
                coordPtr += 2;
                numPoints++;
                current[0]  = close->x;
                current[1]  = close->y;
                isclosed = 1;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }

    if (numPoints > 1) {
        if (isclosed) {
            numStrokes = numPoints - 1;
        } else {
            numStrokes = numPoints;
        }
        inside = SubPathToArea(pathPtr, polyPtr, numPoints, 
                numStrokes, rectPtr, inside);
    }
    
done:
    if (polyPtr != staticSpace) {
        ckfree((char *) polyPtr);
    }

    return inside;
}

/*
 *--------------------------------------------------------------
 *
 * ScalePath --
 *
 *		This procedure is invoked to rescale a line item.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The line referred to by itemPtr is rescaled so that the
 *		following transformation is applied to all point
 *		coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScalePath(
    Tk_Canvas canvas,			/* Canvas containing line. */
    Tk_Item *itemPtr,			/* Line to be scaled. */
    double originX, double originY,	/* Origin about which to scale rect. */
    double scaleX,			/* Amount to scale in X direction. */
    double scaleY)			/* Amount to scale in Y direction. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;
    PathRect r;

    while (atomPtr != NULL) {
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                move->x = originX + scaleX*(move->x - originX);
                move->y = originY + scaleY*(move->y - originY);
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                line->x = originX + scaleX*(line->x - originX);
                line->y = originY + scaleY*(line->y - originY);
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                /* INCOMPLETE !!!!!!!!!!!*/
                /* WRONG !!!!!!!!!!!!!!!*/
                arc->radX = scaleX*arc->radX;
                arc->radY = scaleY*arc->radY;
                arc->x = originX + scaleX*(arc->x - originX);
                arc->y = originY + scaleY*(arc->y - originY);
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                quad->ctrlX = originX + scaleX*(quad->ctrlX - originX);
                quad->ctrlY = originY + scaleY*(quad->ctrlY - originY);
                quad->anchorX = originX + scaleX*(quad->anchorX - originX);
                quad->anchorY = originY + scaleY*(quad->anchorY - originY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                curve->ctrlX1 = originX + scaleX*(curve->ctrlX1 - originX);
                curve->ctrlY1 = originY + scaleY*(curve->ctrlY1 - originY);
                curve->ctrlX2 = originX + scaleX*(curve->ctrlX2 - originX);
                curve->ctrlY2 = originY + scaleY*(curve->ctrlY2 - originY);
                curve->anchorX = originX + scaleX*(curve->anchorX - originX);
                curve->anchorY = originY + scaleY*(curve->anchorY - originY);
                break;
            }
            case PATH_ATOM_Z: {
                CloseAtom *close = (CloseAtom *) atomPtr;
                
                close->x = originX + scaleX*(close->x - originX);
                close->y = originY + scaleY*(close->y - originY);
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    
    /* 
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just scale the bbox'es as well. */
    r = pathPtr->bareBbox;
    r.x1 = originX + scaleX*(r.x1 - originX);
    r.y1 = originY + scaleX*(r.y1 - originY);
    r.x2 = originX + scaleX*(r.x2 - originX);
    r.y2 = originY + scaleX*(r.y2 - originY);
    NormalizePathRect(&r);
    pathPtr->bareBbox = r;
    
    r = pathPtr->totalBbox;
    r.x1 = originX + scaleX*(r.x1 - originX);
    r.y1 = originY + scaleX*(r.y1 - originY);
    r.x2 = originX + scaleX*(r.x2 - originX);
    r.y2 = originY + scaleX*(r.y2 - originY);
    NormalizePathRect(&r);
    pathPtr->bareBbox = r;
}

/*
 *--------------------------------------------------------------
 *
 * TranslatePath --
 *
 *		This procedure is called to move a line by a given amount.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The position of the line is offset by (xDelta, yDelta), and
 *		the bounding box is updated in the generic part of the item
 *		structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslatePath(
    Tk_Canvas canvas, 			/* Canvas containing item. */
    Tk_Item *itemPtr, 			/* Item that is being moved. */
    double deltaX,			/* Amount by which item is to be */
    double deltaY)                      /* moved. */
{
    PathItem *pathPtr = (PathItem *) itemPtr;
    PathAtom *atomPtr = pathPtr->atomPtr;

    while (atomPtr != NULL) {
        switch (atomPtr->type) {
            case PATH_ATOM_M: {
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                move->x += deltaX;
                move->y += deltaY;
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                line->x += deltaX;
                line->y += deltaY;
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                
                arc->x += deltaX;
                arc->y += deltaY;
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                quad->ctrlX += deltaX;
                quad->ctrlY += deltaY;
                quad->anchorX += deltaX;
                quad->anchorY += deltaY;
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                curve->ctrlX1 += deltaX;
                curve->ctrlY1 += deltaY;
                curve->ctrlX2 += deltaX;
                curve->ctrlY2 += deltaY;
                curve->anchorX += deltaX;
                curve->anchorY += deltaY;
                break;
            }
            case PATH_ATOM_Z: {
                CloseAtom *close = (CloseAtom *) atomPtr;
                
                close->x += deltaX;
                close->y += deltaY;
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    
    /* 
     * Set flags bit so we know that PathCoords need to update the
     * normalized path before being used.
     */
    pathPtr->flags |= kPathItemNeedNewNormalizedPath;

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(pathPtr->bareBbox), deltaX, deltaY);
    TranslatePathRect(&(pathPtr->totalBbox), deltaX, deltaY);

    pathPtr->header.x1 = (int) pathPtr->totalBbox.x1;
    pathPtr->header.x2 = (int) pathPtr->totalBbox.x2;
    pathPtr->header.y1 = (int) pathPtr->totalBbox.y1;
    pathPtr->header.y2 = (int) pathPtr->totalBbox.y2;
}

/*
 *--------------------------------------------------------------
 *
 * PathToPostscript --
 *
 *		This procedure is called to generate Postscript for
 *		path items.
 *
 * Results:
 *		The return value is a standard Tcl result.  If an error
 *		occurs in generating Postscript then an error message is
 *		left in the interp's result, replacing whatever used
 *		to be there.  If no error occurs, then Postscript for the
 *		item is appended to the result.
 *
 * Side effects:
 *		None.
 *
 *--------------------------------------------------------------
 */

static int
PathToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
    /* PathItem *pathPtr = (PathItem *) itemPtr; */

    return TCL_OK;
}


