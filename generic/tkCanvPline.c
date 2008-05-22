/*
 * tkCanvPline.c --
 *
 *	This file implements a line canvas item modelled after its
 *	SVG counterpart. See http://www.w3.org/TR/SVG11/.
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

/* Values for the PathItem's flag. */

enum {
    kPlineItemNoBboxCalculation      	= (1L << 0)		/* Inhibit any 'ComputePlineBbox' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PlineItem  {
    Tk_PathItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;	/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    Tcl_Obj *styleObj;		/* Object with style name. */
    PathRect coords;		/* Coordinates (unorders bare bbox). */
    PathRect totalBbox;		/* Bounding box including stroke.
				 * Untransformed coordinates. */
    long flags;			/* Various flags, see enum. */
    char *null;   		/* Just a placeholder for not yet implemented stuff. */ 
} PlineItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePlineBbox(Tk_PathCanvas canvas, PlineItem *plinePtr);
static int	ConfigurePline(Tcl_Interp *interp, Tk_PathCanvas canvas, 
                        Tk_PathItem *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int	CreatePline(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void	DeletePline(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPline(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int	PlineCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	PlineToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static double	PlineToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
static int	PlineToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass);
static void	ScalePline(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void	TranslatePline(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double deltaX, double deltaY);
static PathAtom * MakePathAtoms(PlineItem *plinePtr);

PATH_STYLE_CUSTOM_OPTION_MATRIX
PATH_STYLE_CUSTOM_OPTION_DASH
PATH_CUSTOM_OPTION_TAGS

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(PlineItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_MATRIX(PlineItem),
    PATH_OPTION_SPEC_STYLE_STROKE(PlineItem, "black"),
    PATH_OPTION_SPEC_END
};

static Tk_OptionTable optionTable = NULL;

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPlineType = {
    "pline",				/* name */
    sizeof(PlineItem),			/* itemSize */
    CreatePline,			/* createProc */
    optionSpecs,			/* optionSpecs */
    ConfigurePline,			/* configureProc */
    PlineCoords,			/* coordProc */
    DeletePline,			/* deleteProc */
    DisplayPline,			/* displayProc */
    0,					/* flags */
    PlineToPoint,			/* pointProc */
    PlineToArea,			/* areaProc */
    PlineToPostscript,			/* postscriptProc */
    ScalePline,				/* scaleProc */
    TranslatePline,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};

static int		
CreatePline(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathCreateStyle(&(plinePtr->style));
    plinePtr->canvas = canvas;
    plinePtr->styleObj = NULL;
    plinePtr->totalBbox = NewEmptyPathRect();
    plinePtr->flags = 0L;
    
    if (optionTable == NULL) {
	optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    } 
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) plinePtr, optionTable, 
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    
    /*
     * Since both PlineCoords and ConfigurePline computes new bbox'es
     * we skip this and do it ourself below.
     */
    plinePtr->flags |= kPlineItemNoBboxCalculation;
    if (PlineCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePline(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        plinePtr->flags &= ~kPlineItemNoBboxCalculation;
        ComputePlineBbox(canvas, plinePtr);
        return TCL_OK;
    }

    error:
    DeletePline(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PlineCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    PathRect *p = &(plinePtr->coords);

    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewObj();
        Tcl_Obj *subobj = Tcl_NewDoubleObj(p->x1);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(p->y1);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(p->x2);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        subobj = Tcl_NewDoubleObj(p->y2);
        Tcl_ListObjAppendElement(interp, obj, subobj);
        Tcl_SetObjResult(interp, obj);
    } else if ((objc == 1) || (objc == 4)) {
        if (objc==1) {
            if (Tcl_ListObjGetElements(interp, objv[0], &objc,
                    (Tcl_Obj ***) &objv) != TCL_OK) {
                return TCL_ERROR;
            } else if (objc != 4) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 4", -1));
                return TCL_ERROR;
            }
        }
        if ((Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[0], &(p->x1)) != TCL_OK)
            || (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[1], &(p->y1)) != TCL_OK)
            || (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[2], &(p->x2)) != TCL_OK)
            || (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[3], &(p->y2)) != TCL_OK)) {
            return TCL_ERROR;
        }
        MakePathAtoms(plinePtr);
        if (!(plinePtr->flags & kPlineItemNoBboxCalculation)) {
            ComputePlineBbox(canvas, plinePtr);
        }
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 0 or 4", -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}

static void
ComputePlineBbox(Tk_PathCanvas canvas, PlineItem *plinePtr)
{
    Tk_PathStyle *stylePtr = &(plinePtr->style);
    Tk_PathState state = plinePtr->header.state;
    PathRect r;

    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (state == TK_PATHSTATE_HIDDEN) {
        plinePtr->header.x1 = plinePtr->header.x2 =
        plinePtr->header.y1 = plinePtr->header.y2 = -1;
        return;
    }
    r.x1 = MIN(plinePtr->coords.x1, plinePtr->coords.x2);
    r.x2 = MAX(plinePtr->coords.x1, plinePtr->coords.x2);
    r.y1 = MIN(plinePtr->coords.y1, plinePtr->coords.y2);
    r.y2 = MAX(plinePtr->coords.y1, plinePtr->coords.y2);
    plinePtr->totalBbox = GetGenericPathTotalBboxFromBare(NULL, stylePtr, &r);
    SetGenericPathHeaderBbox(&(plinePtr->header), stylePtr->matrixPtr, &(plinePtr->totalBbox));
}

static int		
ConfigurePline(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(plinePtr->style);
    Tk_PathItem *parentPtr;
    Tk_Window tkwin;
    Tk_PathState state;
    Tk_SavedOptions savedOptions;
    int mask;
    int result;

    tkwin = Tk_PathCanvasTkwin(canvas);
    result = Tk_SetOptions(interp, (char *) plinePtr, optionTable, 
	    objc, objv, tkwin, &savedOptions, &mask);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if ((plinePtr->styleObj != NULL) && (mask & PATH_CORE_OPTION_STYLENAME)) {
        result = TkPathCanvasStyleMergeStyles(tkwin, canvas, stylePtr, plinePtr->styleObj, 0);
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
    Tk_FreeSavedOptions(&savedOptions);
    
    state = itemPtr->state;
    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (state == TK_PATHSTATE_HIDDEN) {
        return TCL_OK;
    }
    
    /*
     * Recompute bounding box for path.
     */
    if (!(plinePtr->flags & kPlineItemNoBboxCalculation)) {
        ComputePlineBbox(canvas, plinePtr);
    }
    return TCL_OK;
}

static PathAtom *
MakePathAtoms(PlineItem *plinePtr)
{
    PathAtom *atomPtr;
                
    atomPtr = NewMoveToAtom(plinePtr->coords.x1, plinePtr->coords.y1);
    atomPtr->nextPtr = NewLineToAtom(plinePtr->coords.x2, plinePtr->coords.y2);
    return atomPtr;
}

static void		
DeletePline(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    Tk_FreeConfigOptions((char *) itemPtr, optionTable, Tk_PathCanvasTkwin(canvas));
}

static void		
DisplayPline(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    PathRect r;
    PathAtom *atomPtr;

    r.x1 = MIN(plinePtr->coords.x1, plinePtr->coords.x2);
    r.x2 = MAX(plinePtr->coords.x1, plinePtr->coords.x2);
    r.y1 = MIN(plinePtr->coords.y1, plinePtr->coords.y2);
    r.y2 = MAX(plinePtr->coords.y1, plinePtr->coords.y2);

    atomPtr = MakePathAtoms(plinePtr);
    TkPathDrawPath(Tk_PathCanvasTkwin(canvas), drawable, atomPtr, &(plinePtr->style), &m, &r);
    TkPathFreeAtoms(atomPtr);
}

static double	
PlineToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    PathAtom *atomPtr;
    double point;
    
    /* @@@ Perhaps we should do a simplified treatment here instead of the generic. */
    atomPtr = MakePathAtoms(plinePtr);
    point = GenericPathToPoint(canvas, itemPtr, &(plinePtr->style), 
            atomPtr, 2, pointPtr);
    TkPathFreeAtoms(atomPtr);
    return point;
}

static int		
PlineToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    PathAtom *atomPtr;
    int area;
    
    /* @@@ Perhaps we should do a simplified treatment here instead of the generic. */
    atomPtr = MakePathAtoms(plinePtr);
    area = GenericPathToArea(canvas, itemPtr, &(plinePtr->style), 
            atomPtr, 2, areaPtr);
    TkPathFreeAtoms(atomPtr);
    return area;
}

static int		
PlineToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePline(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{

}

static void		
TranslatePline(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double deltaX, double deltaY)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(plinePtr->style);

    /* Just translate the bbox as well. */
    TranslatePathRect(&(plinePtr->totalBbox), deltaX, deltaY);
    TranslatePathRect(&(plinePtr->coords), deltaX, deltaY);
    SetGenericPathHeaderBbox(&(plinePtr->header), stylePtr->matrixPtr, &(plinePtr->totalBbox));
}

/*----------------------------------------------------------------------*/

