/*
 * tkCanvEllipse.c --
 *
 *	This file implements a rectangle canvas item modelled after its
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
    kEllipseItemNoNewPathAtoms      	= (1L << 0)		/* Inhibit any 'MakePathAtoms' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct EllipseItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    PathAtom *atomPtr;
    double rx;				/* Radius of corners. */
    double ry;
    PathRect bbox;			/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    PathRect totalBbox;		/* Bounding box including stroke.
                             * Untransformed coordinates. */
    int maxNumSegments;		/* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    long flags;				/* Various flags, see enum. */
    char *null;   			/* Just a placeholder for not yet implemented stuff. */ 
} EllipseItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputeEllipseBbox(Tk_Canvas canvas, EllipseItem *ellPtr);
static int		ConfigureEllipse(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreateEllipse(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeleteEllipse(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayEllipse(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		EllipseCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		EllipseToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	EllipseToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		EllipseToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScaleEllipse(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslateEllipse(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);
static void		MakePathAtoms(EllipseItem *ellPtr);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(EllipseItem),
    PATH_CONFIG_SPEC_STYLE_MATRIX(EllipseItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(EllipseItem),
    PATH_CONFIG_SPEC_CORE(EllipseItem),
    
    {TK_CONFIG_DOUBLE, "-rx", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(EllipseItem, rx), 0, 0},
    {TK_CONFIG_DOUBLE, "-ry", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(EllipseItem, ry), 0, 0},

    PATH_END_CONFIG_SPEC
};

/* @@@ Better way? */
#define SIZEOF_CONFIG_SPEC  	sizeof(configSpecs)/sizeof(Tk_ConfigSpec)
#define PRECT_OPTION_INDEX_RX 	(SIZEOF_CONFIG_SPEC - 3)
#define PRECT_OPTION_INDEX_RY 	(SIZEOF_CONFIG_SPEC - 2)
 
/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkEllipseType = {
    "ellipse",						/* name */
    sizeof(EllipseItem),			/* itemSize */
    CreateEllipse,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigureEllipse,				/* configureProc */
    EllipseCoords,					/* coordProc */
    DeleteEllipse,					/* deleteProc */
    DisplayEllipse,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    EllipseToPoint,					/* pointProc */
    EllipseToArea,					/* areaProc */
    EllipseToPostscript,			/* postscriptProc */
    ScaleEllipse,					/* scaleProc */
    TranslateEllipse,				/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                        

static int		
CreateEllipse(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
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
    Tk_CreatePathStyle(&(ellPtr->style));
    ellPtr->canvas = canvas;
    ellPtr->styleName = NULL;
    ellPtr->atomPtr = NULL;
    ellPtr->bbox = NewEmptyPathRect();
    ellPtr->totalBbox = NewEmptyPathRect();
    ellPtr->maxNumSegments = 100;			/* Crude overestimate. */
    ellPtr->flags = 0L;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    /*
     * Since both EllipseCoords and ConfigureEllipse computes new path atoms
     * we skip this and do it ourself below.
     */
    ellPtr->flags |= kEllipseItemNoNewPathAtoms;
    if (EllipseCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigureEllipse(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        ellPtr->flags &= ~kEllipseItemNoNewPathAtoms;
        MakePathAtoms(ellPtr);
        ComputeEllipseBbox(canvas, ellPtr);
        return TCL_OK;
    }

    error:
    DeleteEllipse(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
EllipseCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    int result;

    result = CoordsForRectangularItems(interp, canvas, &(ellPtr->bbox), objc, objv);
    if ((result == TCL_OK) && (objc == 1) || (objc == 4)) {
        if (!(ellPtr->flags & kEllipseItemNoNewPathAtoms)) {
            MakePathAtoms(ellPtr);
            ComputeEllipseBbox(canvas, ellPtr);
        }
    }
    return result;
}

void
ComputeEllipseBbox(Tk_Canvas canvas, EllipseItem *ellPtr)
{
    Tk_PathStyle *stylePtr = &(ellPtr->style);
    Tk_State state = ellPtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (ellPtr->atomPtr == NULL || (state == TK_STATE_HIDDEN)) {
        ellPtr->header.x1 = ellPtr->header.x2 =
        ellPtr->header.y1 = ellPtr->header.y2 = -1;
        return;
    }
    ellPtr->totalBbox = GetGenericPathTotalBboxFromBare(stylePtr, &(ellPtr->bbox));
    SetGenericPathHeaderBbox(&(ellPtr->header), stylePtr->matrixPtr, &(ellPtr->totalBbox));
}

static int		
ConfigureEllipse(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ellPtr->style);
    Tk_Window tkwin;
    Tk_State state;
    unsigned long mask;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) ellPtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    stylePtr->fillOpacity   = MAX(0.0, MIN(1.0, stylePtr->fillOpacity));
    ellPtr->rx = MAX(0.0, ellPtr->rx);
    ellPtr->ry = MAX(0.0, ellPtr->ry);

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (ellPtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, ellPtr->styleName, 0);
    } 
    
    state = itemPtr->state;
    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        return TCL_OK;
    }
    
    /*
     * Only -rx and -ry affect the path geometry.
     */
    if ((configSpecs[PRECT_OPTION_INDEX_RX].specFlags & TK_CONFIG_OPTION_SPECIFIED) ||
            (configSpecs[PRECT_OPTION_INDEX_RY].specFlags & TK_CONFIG_OPTION_SPECIFIED)) {
        if (!(ellPtr->flags & kEllipseItemNoNewPathAtoms)) {
            MakePathAtoms(ellPtr);
        }
    }
    
    /* 
     * Handle the strokeGC and fillGC used only (?) for Tk drawing. 
     */
    mask = Tk_ConfigPathStylesGC(canvas, itemPtr, stylePtr);

    /*
     * Recompute bounding box for path.
     */
    ComputeEllipseBbox(canvas, ellPtr);
    return TCL_OK;
}

static void
MakePathAtoms(EllipseItem *ellPtr)
{
    PathAtom *atomPtr = NULL;
    int round = 1;
    double epsilon = 1e-6;
    double rx = ellPtr->rx;
    double ry = ellPtr->ry;
    double x = ellPtr->bbox.x1;
    double y = ellPtr->bbox.y1;
    double width = ellPtr->bbox.x2 - x;
    double height = ellPtr->bbox.y2 - y;
    
    /* If only one of rx or ry is zero this implies that both shall be nonzero. */
    if (rx < epsilon && ry < epsilon) {
        round = 0;
    } else if (rx < epsilon) {
        rx = ry;
    } else if (ry < epsilon) {
        ry = rx;
    }
    
    /* There are certain constraints on rx and ry. */
    rx = MIN(rx, width/2.0);
    ry = MIN(ry, height/2.0);
    
    /*
     * Free any old stuff.
     */
    if (ellPtr->atomPtr != NULL) {
        TkPathFreeAtoms(ellPtr->atomPtr);
        ellPtr->atomPtr = NULL;
    }
        
    ellPtr->atomPtr = NewMoveToAtom(x+rx, y);
    atomPtr = ellPtr->atomPtr;

    atomPtr->nextPtr = NewLineToAtom(x+width-rx, y);
    atomPtr = atomPtr->nextPtr;
    if (round) {
        atomPtr->nextPtr = NewArcAtom(rx, ry, 0.0, 0, 1, x+width, y+ry);
        atomPtr = atomPtr->nextPtr;
    }
    atomPtr->nextPtr = NewLineToAtom(x+width, y+height-ry);
    atomPtr = atomPtr->nextPtr;
    if (round) {
        atomPtr->nextPtr = NewArcAtom(rx, ry, 0.0, 0, 1, x+width-rx, y+height);
        atomPtr = atomPtr->nextPtr;
    }
    atomPtr->nextPtr = NewLineToAtom(x+rx, y+height);
    atomPtr = atomPtr->nextPtr;
    if (round) {
        atomPtr->nextPtr = NewArcAtom(rx, ry, 0.0, 0, 1, x, y+height-ry);
        atomPtr = atomPtr->nextPtr;
    }
    atomPtr->nextPtr = NewLineToAtom(x, y+ry);
    atomPtr = atomPtr->nextPtr;
    if (round) {
        atomPtr->nextPtr = NewArcAtom(rx, ry, 0.0, 0, 1, x+rx, y);
        atomPtr = atomPtr->nextPtr;
    }
}

static void		
DeleteEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;

    if (ellPtr->atomPtr != NULL) {
        TkPathFreeAtoms(ellPtr->atomPtr);
        ellPtr->atomPtr = NULL;
    }
}

static void		
DisplayEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    TkPathDrawPath(display, drawable, ellPtr->atomPtr, &(ellPtr->style),
            &m, &(ellPtr->bbox));
}

static double	
EllipseToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    PathAtom *atomPtr = ellPtr->atomPtr;
    Tk_PathStyle *stylePtr = &(ellPtr->style);

    return GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            ellPtr->maxNumSegments, pointPtr);
}

static int		
EllipseToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    
    return GenericPathToArea(canvas, itemPtr, &(ellPtr->style), 
            ellPtr->atomPtr, ellPtr->maxNumSegments, areaPtr);
}

static int		
EllipseToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScaleEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslateEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    PathAtom *atomPtr = ellPtr->atomPtr;

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(ellPtr->bbox), deltaX, deltaY);
    TranslatePathRect(&(ellPtr->totalBbox), deltaX, deltaY);

    ellPtr->header.x1 = (int) ellPtr->totalBbox.x1;
    ellPtr->header.x2 = (int) ellPtr->totalBbox.x2;
    ellPtr->header.y1 = (int) ellPtr->totalBbox.y1;
    ellPtr->header.y2 = (int) ellPtr->totalBbox.y2;
}

/*----------------------------------------------------------------------*/

