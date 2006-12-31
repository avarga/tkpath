/*
 * tkCanvPtext.c --
 *
 *	This file implements a text canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkCanvPathUtil.h"
#include "tkPathCopyTk.h"

/* For debugging. */
extern Tcl_Interp *gInterp;

/*
 * The structure below defines the record for each path item.
 */

typedef struct PtextItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    Tk_PathTextStyle textStyle;
    double x;
    double y;
    PathRect bbox;			/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    char *text;				/* The actual text to display; UTF-8 */
    void *custom;			/* Place holder for platform dependent stuff. */
} PtextItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePtextBbox(Tk_Canvas canvas, PtextItem *ptextPtr);
static int		ConfigurePtext(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePtext(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePtext(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPtext(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		PtextCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PtextToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PtextToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PtextToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePtext(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePtext(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-fontfamily", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PtextItem, textStyle.fontFamily), 
            TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-fontsize", (char *) NULL, (char *) NULL,
        "10.0", Tk_Offset(PtextItem, textStyle.fontSize), 0},
    {TK_CONFIG_STRING, "-text", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PtextItem, text), TK_CONFIG_NULL_OK},
    PATH_CONFIG_SPEC_STYLE_MATRIX(PtextItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(PtextItem, ""),
    PATH_CONFIG_SPEC_STYLE_FILL(PtextItem, "black"),
    PATH_CONFIG_SPEC_CORE(PtextItem),
    PATH_END_CONFIG_SPEC
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPtextType = {
    "ptext",						/* name */
    sizeof(PtextItem),				/* itemSize */
    CreatePtext,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePtext,					/* configureProc */
    PtextCoords,					/* coordProc */
    DeletePtext,					/* deleteProc */
    DisplayPtext,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PtextToPoint,					/* pointProc */
    PtextToArea,					/* areaProc */
    PtextToPostscript,				/* postscriptProc */
    ScalePtext,						/* scaleProc */
    TranslatePtext,					/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                         

static int		
CreatePtext(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }
    gInterp = interp;

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathCreateStyle(&(ptextPtr->style));
    ptextPtr->canvas = canvas;
    ptextPtr->styleName = NULL;
    ptextPtr->bbox = NewEmptyPathRect();
    ptextPtr->text = NULL;
    ptextPtr->textStyle.fontFamily = NULL;
    ptextPtr->textStyle.fontSize = 0.0;
    ptextPtr->custom = NULL;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    if (PtextCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePtext(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        ComputePtextBbox(canvas, ptextPtr);
        return TCL_OK;
    }

error:
    DeletePtext(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PtextCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
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
        if ((Tk_CanvasGetCoordFromObj(interp, canvas, objv[0], &(ptextPtr->x)) != TCL_OK)
            || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[1], &(ptextPtr->y)) != TCL_OK)) {
            return TCL_ERROR;
        }
        ComputePtextBbox(canvas, ptextPtr);
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 2", -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}

void
ComputePtextBbox(Tk_Canvas canvas, PtextItem *ptextPtr)
{
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    Tk_State state = ptextPtr->header.state;
    PathRect bbox, r;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (ptextPtr->text == NULL || (state == TK_STATE_HIDDEN)) {
        ptextPtr->header.x1 = ptextPtr->header.x2 =
        ptextPtr->header.y1 = ptextPtr->header.y2 = -1;
        return;
    }
    r = TkPathTextMeasureBbox(&(ptextPtr->textStyle), ptextPtr->text, ptextPtr->custom);
    // @@@ anchor???
    bbox.x1 = ptextPtr->x;
    bbox.y1 = ptextPtr->y + r.y1;	// r.y1 is negative!
    bbox.x2 = ptextPtr->x + r.x2;
    bbox.y2 = ptextPtr->y + r.y2;
    if (stylePtr->strokeColor) {
        double halfWidth = stylePtr->strokeWidth;
        bbox.x1 -= halfWidth;
        bbox.y1 -= halfWidth;
        bbox.x2 += halfWidth;
        bbox.x2 += halfWidth;
    }
    ptextPtr->bbox = bbox;
    SetGenericPathHeaderBbox(&(ptextPtr->header), stylePtr->matrixPtr, &bbox);
}

static int		
ConfigurePtext(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    Tk_Window tkwin;
    Tk_State state;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) ptextPtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    if (ptextPtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, ptextPtr->styleName, 0);
    }         
    state = itemPtr->state;
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        return TCL_OK;
    }
    TkPathTextConfig(&(ptextPtr->textStyle), ptextPtr->text, &(ptextPtr->custom));
    ComputePtextBbox(canvas, ptextPtr);
    return TCL_OK;
}

static void		
DeletePtext(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    // @@@ Shall the ->text also be freed???
    TkPathTextFree(&(ptextPtr->textStyle), ptextPtr->custom);
}

static void		
DisplayPtext(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    TkPathContext ctx;
    
    if (ptextPtr->text == NULL) {
        return;
    }
    ctx = TkPathInit(Tk_CanvasTkwin(canvas), drawable);
    TkPathPushTMatrix(ctx, &m);
    if (stylePtr->matrixPtr != NULL) {
        TkPathPushTMatrix(ctx, stylePtr->matrixPtr);
    }
    TkPathBeginPath(ctx, stylePtr);
    /* @@@ We need to handle gradients as well here!
           Wait to see what the other APIs have to see.
           Quartz:
    kCGTextFillClip,
    kCGTextStrokeClip,
    kCGTextFillStrokeClip,
    kCGTextClip
    */
    TkPathTextDraw(ctx, &(ptextPtr->textStyle), ptextPtr->x, ptextPtr->y, 
            ptextPtr->text, ptextPtr->custom);
    TkPathEndPath(ctx);
    TkPathFree(ctx);
}

static double	
PtextToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    return PathRectToPointWithMatrix(ptextPtr->bbox, stylePtr->matrixPtr, pointPtr);    
}

static int		
PtextToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    return PathRectToAreaWithMatrix(ptextPtr->bbox, stylePtr->matrixPtr, areaPtr);
}

static int		
PtextToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePtext(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* Skip? */
}

static void		
TranslatePtext(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    PtextItem *ptextPtr = (PtextItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ptextPtr->style);
    ptextPtr->x += deltaX;
    ptextPtr->y += deltaY;
    TranslatePathRect(&(ptextPtr->bbox), deltaX, deltaY);
    SetGenericPathHeaderBbox(&(ptextPtr->header), stylePtr->matrixPtr, &(ptextPtr->bbox));
}

/*----------------------------------------------------------------------*/

