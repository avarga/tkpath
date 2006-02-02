/*
 * tkCanvPpolygon.c --
 *
 *	This file implements a polygon canvas item modelled after its
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

/* Values for the PpolygonItem's flag. */

enum {
    kPpolygonItemNoBboxCalculation     	= (1L << 0)		/* Inhibit any 'ComputePpolygonBbox' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PpolygonItem  {
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
} PpolygonItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePpolygonBbox(Tk_Canvas canvas, PpolygonItem *polylinePtr);
static int		ConfigurePpolygon(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePpolygon(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePpolygon(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPpolygon(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		PpolygonCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PpolygonToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PpolygonToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PpolygonToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePpolygon(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePpolygon(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(PpolygonItem),
    PATH_CONFIG_SPEC_STYLE_MATRIX(PpolygonItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(PpolygonItem),
    PATH_CONFIG_SPEC_CORE(PpolygonItem),
    PATH_END_CONFIG_SPEC
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPpolygonType = {
    "ppolygon",						/* name */
    sizeof(PpolygonItem),			/* itemSize */
    CreatePpolygon,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePpolygon,				/* configureProc */
    PpolygonCoords,					/* coordProc */
    DeletePpolygon,					/* deleteProc */
    DisplayPpolygon,				/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PpolygonToPoint,				/* pointProc */
    PpolygonToArea,					/* areaProc */
    PpolygonToPostscript,			/* postscriptProc */
    ScalePpolygon,					/* scaleProc */
    TranslatePpolygon,				/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                        
 

static int		
CreatePpolygon(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
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
    Tk_CreatePathStyle(&(polylinePtr->style));
    polylinePtr->canvas = canvas;
    polylinePtr->styleName = NULL;
    polylinePtr->atomPtr = NULL;
    polylinePtr->bbox = NewEmptyPathRect();
    polylinePtr->totalBbox = NewEmptyPathRect();
    polylinePtr->maxNumSegments = 0;
    polylinePtr->flags = 0L;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    
    /*
     * Since both PpolygonCoords and ConfigurePpolygon computes new bbox'es
     * we skip this and do it ourself below.
     */
    polylinePtr->flags |= kPpolygonItemNoBboxCalculation;
    if (PpolygonCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePpolygon(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        polylinePtr->flags &= ~kPpolygonItemNoBboxCalculation;
        ComputePpolygonBbox(canvas, polylinePtr);
        return TCL_OK;
    }

    error:
    DeletePpolygon(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PpolygonCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    int len;

    if (CoordsForPolygonline(interp, canvas, 1, objc, objv, 
            &(polylinePtr->atomPtr), &len) != TCL_OK) {
        return TCL_ERROR;
    }
    polylinePtr->maxNumSegments = len;
    if (!(polylinePtr->flags & kPpolygonItemNoBboxCalculation)) {
        ComputePpolygonBbox(canvas, polylinePtr);
    }
    return TCL_OK;
}	

void
ComputePpolygonBbox(Tk_Canvas canvas, PpolygonItem *polylinePtr)
{
    Tk_PathStyle *stylePtr = &(polylinePtr->style);
    Tk_State state = polylinePtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (polylinePtr->atomPtr == NULL || (state == TK_STATE_HIDDEN)) {
        polylinePtr->header.x1 = polylinePtr->header.x2 =
        polylinePtr->header.y1 = polylinePtr->header.y2 = -1;
        return;
    }
    polylinePtr->bbox = GetGenericBarePathBbox(polylinePtr->atomPtr);
    polylinePtr->totalBbox = GetGenericPathTotalBboxFromBare(stylePtr, &(polylinePtr->bbox));
    SetGenericPathHeaderBbox(&(polylinePtr->header), stylePtr->matrixPtr, &(polylinePtr->totalBbox));
}

static int		
ConfigurePpolygon(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(polylinePtr->style);
    Tk_Window tkwin;
    Tk_State state;
    unsigned long mask;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) polylinePtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (polylinePtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, polylinePtr->styleName, 
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
    if (!(polylinePtr->flags & kPpolygonItemNoBboxCalculation)) {
        ComputePpolygonBbox(canvas, polylinePtr);
    }
    return TCL_OK;
}

static void		
DeletePpolygon(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;

    if (polylinePtr->atomPtr != NULL) {
        TkPathFreeAtoms(polylinePtr->atomPtr);
        polylinePtr->atomPtr = NULL;
    }
}

static void		
DisplayPpolygon(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    TkPathDrawPath(display, drawable, polylinePtr->atomPtr, &(polylinePtr->style),
            &m, &(polylinePtr->bbox));
}

static double	
PpolygonToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    PathAtom *atomPtr = polylinePtr->atomPtr;
    Tk_PathStyle *stylePtr = &(polylinePtr->style);

    return GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            polylinePtr->maxNumSegments, pointPtr);
}

static int		
PpolygonToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    
    return GenericPathToArea(canvas, itemPtr, &(polylinePtr->style), 
            polylinePtr->atomPtr, polylinePtr->maxNumSegments, areaPtr);
}

static int		
PpolygonToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePpolygon(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslatePpolygon(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    PpolygonItem *polylinePtr = (PpolygonItem *) itemPtr;
    PathAtom *atomPtr = polylinePtr->atomPtr;

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(polylinePtr->bbox), deltaX, deltaY);
    TranslatePathRect(&(polylinePtr->totalBbox), deltaX, deltaY);

    polylinePtr->header.x1 = (int) polylinePtr->totalBbox.x1;
    polylinePtr->header.x2 = (int) polylinePtr->totalBbox.x2;
    polylinePtr->header.y1 = (int) polylinePtr->totalBbox.y1;
    polylinePtr->header.y2 = (int) polylinePtr->totalBbox.y2;
}

/*----------------------------------------------------------------------*/

