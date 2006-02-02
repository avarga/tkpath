/*
 * tkCanvPline.c --
 *
 *	This file implements a line canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2006  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkCanvPathUtil.h"
#include "tkPathCopyTk.h"

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
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    PathAtom *atomPtr;
    PathRect bbox;			/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    PathRect totalBbox;		/* Bounding box including stroke.
                             * Untransformed coordinates. */
    int maxNumSegments;		/* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    long flags;				/* Various flags, see enum. */
    char *null;   			/* Just a placeholder for not yet implemented stuff. */ 
} PlineItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePlineBbox(Tk_Canvas canvas, PlineItem *plinePtr);
static int		ConfigurePline(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePline(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePline(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPline(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		PlineCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PlineToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PlineToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PlineToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePline(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePline(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);
static void		MakePathAtoms(PlineItem *plinePtr);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    PATH_CONFIG_SPEC_STYLE_MATRIX(PlineItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(PlineItem),
    PATH_CONFIG_SPEC_CORE(PlineItem),
    PATH_END_CONFIG_SPEC
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPlineType = {
    "pline",						/* name */
    sizeof(PlineItem),				/* itemSize */
    CreatePline,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePline,					/* configureProc */
    PlineCoords,					/* coordProc */
    DeletePline,					/* deleteProc */
    DisplayPline,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PlineToPoint,					/* pointProc */
    PlineToArea,					/* areaProc */
    PlineToPostscript,				/* postscriptProc */
    ScalePline,						/* scaleProc */
    TranslatePline,					/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                        
 

static int		
CreatePline(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
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
    Tk_CreatePathStyle(&(plinePtr->style));
    plinePtr->canvas = canvas;
    plinePtr->styleName = NULL;
    plinePtr->atomPtr = NULL;
    plinePtr->bbox = NewEmptyPathRect();
    plinePtr->totalBbox = NewEmptyPathRect();
    plinePtr->maxNumSegments = 2;
    plinePtr->flags = 0L;
    
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
    DeletePline(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PlineCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    int result;

    result = CoordsForRectangularItems(interp, canvas, &(plinePtr->bbox), objc, objv);
    if ((result == TCL_OK) && (objc == 1) || (objc == 4)) {
        MakePathAtoms(plinePtr);
        if (!(plinePtr->flags & kPlineItemNoBboxCalculation)) {
            ComputePlineBbox(canvas, plinePtr);
        }
    }
    return result;
}

void
ComputePlineBbox(Tk_Canvas canvas, PlineItem *plinePtr)
{
    Tk_PathStyle *stylePtr = &(plinePtr->style);
    Tk_State state = plinePtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (plinePtr->atomPtr == NULL || (state == TK_STATE_HIDDEN)) {
        plinePtr->header.x1 = plinePtr->header.x2 =
        plinePtr->header.y1 = plinePtr->header.y2 = -1;
        return;
    }
    plinePtr->totalBbox = GetGenericPathTotalBboxFromBare(stylePtr, &(plinePtr->bbox));
    SetGenericPathHeaderBbox(&(plinePtr->header), stylePtr->matrixPtr, &(plinePtr->totalBbox));
}

static int		
ConfigurePline(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(plinePtr->style);
    Tk_Window tkwin;
    Tk_State state;
    unsigned long mask;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) plinePtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (plinePtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, plinePtr->styleName, 
                kPathMergeStyleNotFill);
    }     
    
    state = itemPtr->state;
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        return TCL_OK;
    }
    
    /* 
     * Handle the strokeGC and fillGC used only (?) for Tk drawing. 
     */
    mask = Tk_ConfigPathStylesGC(canvas, itemPtr, stylePtr);

    /*
     * Recompute bounding box for path.
     */
    if (!(plinePtr->flags & kPlineItemNoBboxCalculation)) {
        ComputePlineBbox(canvas, plinePtr);
    }
    return TCL_OK;
}

static void
MakePathAtoms(PlineItem *plinePtr)
{
    PathAtom *atomPtr = NULL;
        
    /*
     * Free any old stuff.
     */
    if (plinePtr->atomPtr != NULL) {
        TkPathFreeAtoms(plinePtr->atomPtr);
        plinePtr->atomPtr = NULL;
    }
        
    plinePtr->atomPtr = NewMoveToAtom(plinePtr->bbox.x1, plinePtr->bbox.y1);
    atomPtr = plinePtr->atomPtr;
    atomPtr->nextPtr = NewLineToAtom(plinePtr->bbox.x2, plinePtr->bbox.y2);
    atomPtr = atomPtr->nextPtr;
}

static void		
DeletePline(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;

    if (plinePtr->atomPtr != NULL) {
        TkPathFreeAtoms(plinePtr->atomPtr);
        plinePtr->atomPtr = NULL;
    }
}

static void		
DisplayPline(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    TkPathDrawPath(display, drawable, plinePtr->atomPtr, &(plinePtr->style),
            &m, &(plinePtr->bbox));
}

static double	
PlineToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    PathAtom *atomPtr = plinePtr->atomPtr;
    Tk_PathStyle *stylePtr = &(plinePtr->style);

    return GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            plinePtr->maxNumSegments, pointPtr);
}

static int		
PlineToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    
    return GenericPathToArea(canvas, itemPtr, &(plinePtr->style), 
            plinePtr->atomPtr, plinePtr->maxNumSegments, areaPtr);
}

static int		
PlineToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePline(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslatePline(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    PlineItem *plinePtr = (PlineItem *) itemPtr;
    PathAtom *atomPtr = plinePtr->atomPtr;

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(plinePtr->bbox), deltaX, deltaY);
    TranslatePathRect(&(plinePtr->totalBbox), deltaX, deltaY);

    plinePtr->header.x1 = (int) plinePtr->totalBbox.x1;
    plinePtr->header.x2 = (int) plinePtr->totalBbox.x2;
    plinePtr->header.y1 = (int) plinePtr->totalBbox.y1;
    plinePtr->header.y2 = (int) plinePtr->totalBbox.y2;
}

/*----------------------------------------------------------------------*/

