/*
 * tkCanvPtext.c --
 *
 *	This file implements a text canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007-2008  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkpCanvas.h"
#include "tkCanvPathUtil.h"
#include "tkPathStyle.h"

/* For debugging. */
extern Tcl_Interp *gInterp;

enum {
    kPtextItemNoBboxCalculation      	= (1L << 0)		/* Inhibit any 'ComputePtextBbox' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PtextItem  {
    Tk_PathItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;	/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    Tcl_Obj *styleObj;		/* Object with style name. */
    Tk_PathTextStyle textStyle;
    int textAnchor;
    double x;
    double y;
    PathRect bbox;		/* Bounding box with zero width outline.
				 * Untransformed coordinates. */
    Tcl_Obj *utf8Obj;		/* The actual text to display; UTF-8 */
    int numChars;		/* Length of text in characters. */
    int numBytes;		/* Length of text in bytes. */
    long flags;			/* Various flags, see enum. */
    void *custom;		/* Place holder for platform dependent stuff. */
} PtextItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePtextBbox(Tk_PathCanvas canvas, PtextItem *ptextPtr);
static int	ConfigurePtext(Tcl_Interp *interp, Tk_PathCanvas canvas, 
                        Tk_PathItem *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int	CreatePtext(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void	DeletePtext(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPtext(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int	PtextCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	PtextToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static double	PtextToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
static int	PtextToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass);
static void	ScalePtext(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void	TranslatePtext(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double deltaX, double deltaY);
static void	PtextGradientProc(ClientData clientData, int flags);
#if 0
static void	PtextDeleteChars(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
                        int first, int last);
#endif

enum {
    PRECT_OPTION_INDEX_FONTFAMILY	    = (1L << (PATH_STYLE_OPTION_INDEX_END + 0)),
    PRECT_OPTION_INDEX_FONTSIZE		    = (1L << (PATH_STYLE_OPTION_INDEX_END + 1)),
    PRECT_OPTION_INDEX_TEXT		    = (1L << (PATH_STYLE_OPTION_INDEX_END + 2)),
    PRECT_OPTION_INDEX_TEXTANCHOR	    = (1L << (PATH_STYLE_OPTION_INDEX_END + 3)),
};
 
PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS

/*
 * The enum kPathTextAnchorStart... MUST be kept in sync!
 */
static char *textAnchorST[] = {
    "start", "middle", "end", NULL
};

// @@@ TODO: have platform specific default font family.
#define PATH_OPTION_SPEC_FONTFAMILY		    \
    {TK_OPTION_STRING, "-fontfamily", NULL, NULL,   \
        "Helvetica", -1, Tk_Offset(PtextItem, textStyle.fontFamily),   \
	0, 0, PRECT_OPTION_INDEX_FONTFAMILY}

#define PATH_OPTION_SPEC_FONTSIZE		    \
    {TK_OPTION_DOUBLE, "-fontsize", NULL, NULL,   \
        "12.0", -1, Tk_Offset(PtextItem, textStyle.fontSize),   \
	0, 0, PRECT_OPTION_INDEX_FONTSIZE}

#define PATH_OPTION_SPEC_TEXT		    \
    {TK_OPTION_STRING, "-text", NULL, NULL,   \
        NULL, Tk_Offset(PtextItem, utf8Obj), -1,  \
	TK_OPTION_NULL_OK, 0, PRECT_OPTION_INDEX_TEXT}
	
#define PATH_OPTION_SPEC_TEXTANCHOR		    \
    {TK_OPTION_STRING_TABLE, "-textanchor", NULL, NULL, \
        "start", -1, Tk_Offset(PtextItem, textAnchor),	\
        0, (ClientData) textAnchorST, 0}

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(PtextItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(PtextItem, "black"),
    PATH_OPTION_SPEC_STYLE_MATRIX(PtextItem),
    PATH_OPTION_SPEC_STYLE_STROKE(PtextItem, ""),
    PATH_OPTION_SPEC_FONTFAMILY,
    PATH_OPTION_SPEC_FONTSIZE,
    PATH_OPTION_SPEC_TEXT,
    PATH_OPTION_SPEC_TEXTANCHOR,
    PATH_OPTION_SPEC_END
};

static Tk_OptionTable optionTable = NULL;

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPtextType = {
    "ptext",				/* name */
    sizeof(PtextItem),			/* itemSize */
    CreatePtext,			/* createProc */
    optionSpecs,			/* configSpecs */
    ConfigurePtext,			/* configureProc */
    PtextCoords,			/* coordProc */
    DeletePtext,			/* deleteProc */
    DisplayPtext,			/* displayProc */
    0,					/* flags */
    PtextToPoint,			/* pointProc */
    PtextToArea,			/* areaProc */
    PtextToPostscript,			/* postscriptProc */
    ScalePtext,				/* scaleProc */
    TranslatePtext,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};
                         

static int		
CreatePtext(Tcl_Interp *interp, Tk_PathCanvas canvas, 
	struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathCreateStyle(&(ptextPtr->style));
    ptextPtr->canvas = canvas;
    ptextPtr->styleObj = NULL;
    ptextPtr->bbox = NewEmptyPathRect();
    ptextPtr->utf8Obj = NULL;
    ptextPtr->numChars = 0;
    ptextPtr->numBytes = 0;
    ptextPtr->textAnchor = kPathTextAnchorStart;
    ptextPtr->textStyle.fontFamily = NULL;
    ptextPtr->textStyle.fontSize = 0.0;
    ptextPtr->flags = 0L;
    ptextPtr->custom = NULL;
    
    if (optionTable == NULL) {
	optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    } 
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) ptextPtr, optionTable, 
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    ptextPtr->flags |= kPtextItemNoBboxCalculation;
    if (PtextCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePtext(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        ptextPtr->flags &= ~kPtextItemNoBboxCalculation;
        ComputePtextBbox(canvas, ptextPtr);
        return TCL_OK;
    }

error:
    DeletePtext(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PtextCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;

    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewObj();
        Tcl_Obj *subobj = Tcl_NewDoubleObj(ptextPtr->x);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(ptextPtr->y);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        Tcl_SetObjResult(interp, obj);
    } else if (objc < 3) {
        if (objc == 1) {
            if (Tcl_ListObjGetElements(interp, objv[0], &objc,
                    (Tcl_Obj ***) &objv) != TCL_OK) {
                return TCL_ERROR;
            } else if (objc != 2) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 2", -1));
                return TCL_ERROR;
            }
        }
        if ((Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[0], &(ptextPtr->x)) != TCL_OK)
            || (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[1], &(ptextPtr->y)) != TCL_OK)) {
            return TCL_ERROR;
        }
        if (!(ptextPtr->flags & kPtextItemNoBboxCalculation)) {
            ComputePtextBbox(canvas, ptextPtr);
        }
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 2", -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}

void
ComputePtextBbox(Tk_PathCanvas canvas, PtextItem *ptextPtr)
{
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    Tk_PathState state = ptextPtr->header.state;
    double width;
    PathRect bbox, r;

    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (ptextPtr->utf8Obj == NULL || (state == TK_PATHSTATE_HIDDEN)) {
        ptextPtr->header.x1 = ptextPtr->header.x2 =
        ptextPtr->header.y1 = ptextPtr->header.y2 = -1;
        return;
    }
    r = TkPathTextMeasureBbox(&ptextPtr->textStyle, 
	    Tcl_GetString(ptextPtr->utf8Obj), ptextPtr->custom);
    width = r.x2 - r.x1;
    switch (ptextPtr->textAnchor) {
        case kPathTextAnchorStart: 
            bbox.x1 = ptextPtr->x;
            bbox.x2 = bbox.x1 + width;
            break;
        case kPathTextAnchorMiddle:
            bbox.x1 = ptextPtr->x - width/2;
            bbox.x2 = ptextPtr->x + width/2;
            break;
        case kPathTextAnchorEnd:
            bbox.x1 = ptextPtr->x - width;
            bbox.x2 = ptextPtr->x;
            break;
    }
    bbox.y1 = ptextPtr->y + r.y1;	// r.y1 is negative!
    bbox.y2 = ptextPtr->y + r.y2;
    
    /* Fudge for antialiasing etc. */
    bbox.x1 -= 1.0;
    bbox.y1 -= 1.0;
    bbox.x2 += 1.0;
    bbox.y2 += 1.0;
    if (stylePtr->strokeColor) {
        double halfWidth = stylePtr->strokeWidth/2;
        bbox.x1 -= halfWidth;
        bbox.y1 -= halfWidth;
        bbox.x2 += halfWidth;
        bbox.x2 += halfWidth;
    }
    ptextPtr->bbox = bbox;
    SetGenericPathHeaderBbox(&ptextPtr->header, stylePtr->matrixPtr, &bbox);
}

static int		
ConfigurePtext(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    Tk_PathItem *parentPtr;
    Tk_Window tkwin;
    Tk_PathState state;
    Tk_SavedOptions savedOptions;
    int mask;
    int result;

    tkwin = Tk_PathCanvasTkwin(canvas);
    result = Tk_SetOptions(interp, (char *) ptextPtr, optionTable, 
	    objc, objv, tkwin, &savedOptions, &mask);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if ((ptextPtr->styleObj != NULL) && (mask & PATH_CORE_OPTION_STYLENAME)) {
        result = TkPathCanvasStyleMergeStyles(tkwin, canvas, stylePtr, ptextPtr->styleObj, 0);
	if (result != TCL_OK) {
	    Tk_RestoreSavedOptions(&savedOptions);
	    return result;
	}
    } 
    if (mask & PATH_CORE_OPTION_PARENT) {
	result = TkPathCanvasFindGroup(interp, canvas, itemPtr->parentObj, &parentPtr);
	if (result != TCL_OK) {
	    Tk_RestoreSavedOptions(&savedOptions);
	    return result;
	}
	TkPathCanvasSetParent(parentPtr, itemPtr);
    }
    
    /*
     * Just translate the 'fillObj' (string) to a TkPathColor.
     * We MUST have this last in the chain of custom option checks!
     */
    if (mask & PATH_STYLE_OPTION_FILL) {
	TkPathColor *fillPtr = NULL;

	if (stylePtr->fillObj != NULL) {
	    fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->fillObj,
		    TkPathCanvasGradientTable(canvas), PtextGradientProc,
		    (ClientData) itemPtr);
	    if (fillPtr == NULL) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	} else {
	    fillPtr = NULL;
	}
	/* Free any old and store the new. */
	if (stylePtr->fill != NULL) {
	    TkPathFreePathColor(stylePtr->fill);
	}
	stylePtr->fill = fillPtr;
    }
    Tk_FreeSavedOptions(&savedOptions);
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    if (ptextPtr->styleObj != NULL) {
	TkPathCanvasStyleMergeStyles(tkwin, canvas, stylePtr, ptextPtr->styleObj, 0);
    }         
    state = itemPtr->state;
    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (ptextPtr->utf8Obj != NULL) {
        ptextPtr->numBytes = Tcl_GetCharLength(ptextPtr->utf8Obj);
        ptextPtr->numChars = Tcl_NumUtfChars(Tcl_GetString(ptextPtr->utf8Obj), 
		ptextPtr->numBytes);
    } else {
        ptextPtr->numBytes = 0;
        ptextPtr->numChars = 0;
    }
    if (state == TK_PATHSTATE_HIDDEN) {
        return TCL_OK;
    }
    // @@@ TkPathTextConfig needs to be reworked!
    if (TkPathTextConfig(interp, &(ptextPtr->textStyle), 
	    Tcl_GetString(ptextPtr->utf8Obj), &(ptextPtr->custom)) == TCL_OK) {
        if (!(ptextPtr->flags & kPtextItemNoBboxCalculation)) {
            ComputePtextBbox(canvas, ptextPtr);
        }
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}

static void		
DeletePtext(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    TkPathTextFree(&(ptextPtr->textStyle), ptextPtr->custom);
    Tk_FreeConfigOptions((char *) ptextPtr, optionTable, 
	    Tk_PathCanvasTkwin(canvas));
}

static void		
DisplayPtext(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    TkPathContext ctx;
    
    if (ptextPtr->utf8Obj == NULL) {
        return;
    }
    ctx = TkPathInit(Tk_PathCanvasTkwin(canvas), drawable);
    TkPathPushTMatrix(ctx, &m);
    if (stylePtr->matrixPtr != NULL) {
        TkPathPushTMatrix(ctx, stylePtr->matrixPtr);
    }
    TkPathBeginPath(ctx, stylePtr);
    /* @@@ We need to handle gradients as well here!
           Wait to see what the other APIs have to say.
    */
    TkPathTextDraw(ctx, stylePtr, &(ptextPtr->textStyle), ptextPtr->bbox.x1, ptextPtr->y, 
            Tcl_GetString(ptextPtr->utf8Obj), ptextPtr->custom);
    TkPathEndPath(ctx);
    TkPathFree(ctx);
}

static double	
PtextToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    return PathRectToPointWithMatrix(ptextPtr->bbox, stylePtr->matrixPtr, pointPtr);    
}

static int		
PtextToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    return PathRectToAreaWithMatrix(ptextPtr->bbox, stylePtr->matrixPtr, areaPtr);
}

static int		
PtextToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePtext(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* Skip? */
}

static void		
TranslatePtext(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double deltaX, double deltaY)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    ptextPtr->x += deltaX;
    ptextPtr->y += deltaY;
    TranslatePathRect(&(ptextPtr->bbox), deltaX, deltaY);
    SetGenericPathHeaderBbox(&(ptextPtr->header), stylePtr->matrixPtr, &(ptextPtr->bbox));
}

static void	
PtextGradientProc(ClientData clientData, int flags)
{
    PtextItem *ptextPtr = (PtextItem *)clientData;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
        
    if (flags) {
	if (flags & PATH_GRADIENT_FLAG_DELETE) {
	    TkPathFreePathColor(stylePtr->fill);	
	    stylePtr->fill = NULL;
	    Tcl_DecrRefCount(stylePtr->fillObj);
	    stylePtr->fillObj = NULL;
	}
	Tk_PathCanvasEventuallyRedraw(ptextPtr->canvas,
		ptextPtr->header.x1, ptextPtr->header.y1,
		ptextPtr->header.x2, ptextPtr->header.y2);
    }
}

#if 0	// TODO
static void
PtextDeleteChars(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int first, int last)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    int byteIndex, byteCount, charsRemoved;
    char *new, *text;

    text = ptextPtr->utf8;
    if (first < 0) {
        first = 0;
    }
    if (last >= ptextPtr->numChars) {
        last = ptextPtr->numChars - 1;
    }
    if (first > last) {
        return;
    }
    charsRemoved = last + 1 - first;

    byteIndex = Tcl_UtfAtIndex(text, first) - text;
    byteCount = Tcl_UtfAtIndex(text + byteIndex, charsRemoved) - (text + byteIndex);
    
    new = (char *) ckalloc((unsigned) (ptextPtr->numBytes + 1 - byteCount));
    memcpy(new, text, (size_t) byteIndex);
    strcpy(new + byteIndex, text + byteIndex + byteCount);

    ckfree(text);
    ptextPtr->utf8 = new;
    ptextPtr->numChars -= charsRemoved;
    ptextPtr->numBytes -= byteCount;
    
    //TkPathTextConfig(interp, &(ptextPtr->textStyle), ptextPtr->utf8, &(ptextPtr->custom));
    ComputePtextBbox(canvas, ptextPtr);
    return;
}
#endif

/*----------------------------------------------------------------------*/

