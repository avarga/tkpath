/*
 * tkCanvPrect.c --
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
    kPrectItemNoNewPathAtoms      	= (1L << 0)		/* Inhibit any 'MakePathAtoms' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PrectItem  {
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
} PrectItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePrectBbox(Tk_Canvas canvas, PrectItem *prectPtr);
static int		ConfigurePrect(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePrect(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePrect(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPrect(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		PrectCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PrectToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PrectToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PrectToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePrect(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePrect(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);
static void		MakePathAtoms(PrectItem *prectPtr);


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(PrectItem),
    PATH_CONFIG_SPEC_STYLE_MATRIX(PrectItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(PrectItem),
    PATH_CONFIG_SPEC_CORE(PrectItem),
    
    {TK_CONFIG_DOUBLE, "-rx", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(PrectItem, rx), 0, 0},
    {TK_CONFIG_DOUBLE, "-ry", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(PrectItem, ry), 0, 0},

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

Tk_ItemType tkPrectType = {
    "prect",						/* name */
    sizeof(PrectItem),				/* itemSize */
    CreatePrect,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePrect,					/* configureProc */
    PrectCoords,					/* coordProc */
    DeletePrect,					/* deleteProc */
    DisplayPrect,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PrectToPoint,					/* pointProc */
    PrectToArea,					/* areaProc */
    PrectToPostscript,				/* postscriptProc */
    ScalePrect,						/* scaleProc */
    TranslatePrect,					/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                        

static int		
CreatePrect(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
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
    Tk_CreatePathStyle(&(prectPtr->style));
    prectPtr->canvas = canvas;
    prectPtr->styleName = NULL;
    prectPtr->atomPtr = NULL;
    prectPtr->bbox = NewEmptyPathRect();
    prectPtr->totalBbox = NewEmptyPathRect();
    prectPtr->maxNumSegments = 100;			/* Crude overestimate. */
    prectPtr->flags = 0L;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    /*
     * Since both PrectCoords and ConfigurePrect computes new path atoms
     * we skip this and do it ourself below.
     */
    prectPtr->flags |= kPrectItemNoNewPathAtoms;
    if (PrectCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePrect(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        prectPtr->flags &= ~kPrectItemNoNewPathAtoms;
        MakePathAtoms(prectPtr);
        ComputePrectBbox(canvas, prectPtr);
        return TCL_OK;
    }

    error:
    DeletePrect(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PrectCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    int result;

    result = CoordsForRectangularItems(interp, canvas, &(prectPtr->bbox), objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 4))) {
        if (!(prectPtr->flags & kPrectItemNoNewPathAtoms)) {
            MakePathAtoms(prectPtr);
            ComputePrectBbox(canvas, prectPtr);
        }
    }
    return result;
}

void
ComputePrectBbox(Tk_Canvas canvas, PrectItem *prectPtr)
{
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    Tk_State state = prectPtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (prectPtr->atomPtr == NULL || (state == TK_STATE_HIDDEN)) {
        prectPtr->header.x1 = prectPtr->header.x2 =
        prectPtr->header.y1 = prectPtr->header.y2 = -1;
        return;
    }
    prectPtr->totalBbox = GetGenericPathTotalBboxFromBare(NULL, stylePtr, &(prectPtr->bbox));
    SetGenericPathHeaderBbox(&(prectPtr->header), stylePtr->matrixPtr, &(prectPtr->totalBbox));
}

static int		
ConfigurePrect(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    Tk_Window tkwin;
    Tk_State state;
    unsigned long mask;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) prectPtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    stylePtr->fillOpacity   = MAX(0.0, MIN(1.0, stylePtr->fillOpacity));
    prectPtr->rx = MAX(0.0, prectPtr->rx);
    prectPtr->ry = MAX(0.0, prectPtr->ry);

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (prectPtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, prectPtr->styleName, 0);
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
        if (!(prectPtr->flags & kPrectItemNoNewPathAtoms)) {
            MakePathAtoms(prectPtr);
        }
    }
    
    /* 
     * Handle the strokeGC and fillGC used only (?) for Tk drawing. 
     */
    mask = Tk_ConfigPathStylesGC(canvas, itemPtr, stylePtr);

    /*
     * Recompute bounding box for path.
     */
    ComputePrectBbox(canvas, prectPtr);
    return TCL_OK;
}

static void
MakePathAtoms(PrectItem *prectPtr)
{
    PathAtom *atomPtr = NULL;
    int round = 1;
    double epsilon = 1e-6;
    double rx = prectPtr->rx;
    double ry = prectPtr->ry;
    double x = prectPtr->bbox.x1;
    double y = prectPtr->bbox.y1;
    double width = prectPtr->bbox.x2 - x;
    double height = prectPtr->bbox.y2 - y;
    
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
    if (prectPtr->atomPtr != NULL) {
        TkPathFreeAtoms(prectPtr->atomPtr);
        prectPtr->atomPtr = NULL;
    }
        
    prectPtr->atomPtr = NewMoveToAtom(x+rx, y);
    atomPtr = prectPtr->atomPtr;

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
DeletePrect(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;

    if (prectPtr->atomPtr != NULL) {
        TkPathFreeAtoms(prectPtr->atomPtr);
        prectPtr->atomPtr = NULL;
    }
}

static void		
DisplayPrect(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    TkPathDrawPath(Tk_CanvasTkwin(canvas), drawable, prectPtr->atomPtr, &(prectPtr->style),
            &m, &(prectPtr->bbox));
}

static double	
PrectToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    PathAtom *atomPtr = prectPtr->atomPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    PathRect *bboxPtr = &(prectPtr->bbox);
    double bareRect[4];
    double width, dist;
    int rectiLinear = 0;
    int filled;

    filled = stylePtr->fillColor != NULL;
    width = 0.0;
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
    }
    
    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = bboxPtr->x1;
            bareRect[1] = bboxPtr->y1;
            bareRect[2] = bboxPtr->x2;
            bareRect[3] = bboxPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
        
            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * bboxPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * bboxPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * bboxPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * bboxPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        dist = PathRectToPoint(bareRect, width, filled, pointPtr);
    } else {
        dist = GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            prectPtr->maxNumSegments, pointPtr);
    }
    return dist;
}

static int		
PrectToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    PathRect *bboxPtr = &(prectPtr->bbox);
    double bareRect[4];
    double width;
    int rectiLinear = 0;
    int filled;

    filled = stylePtr->fillColor != NULL;
    width = 0.0;
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
    }

    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = bboxPtr->x1;
            bareRect[1] = bboxPtr->y1;
            bareRect[2] = bboxPtr->x2;
            bareRect[3] = bboxPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
        
            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * bboxPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * bboxPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * bboxPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * bboxPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        return PathRectToArea(bareRect, width, filled, areaPtr);
    } else {
        return GenericPathToArea(canvas, itemPtr, &(prectPtr->style), 
                prectPtr->atomPtr, prectPtr->maxNumSegments, areaPtr);
    }
}

static int		
PrectToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePrect(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslatePrect(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    PathAtom *atomPtr = prectPtr->atomPtr;

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(prectPtr->bbox), deltaX, deltaY);
    TranslatePathRect(&(prectPtr->totalBbox), deltaX, deltaY);

    prectPtr->header.x1 = (int) prectPtr->totalBbox.x1;
    prectPtr->header.x2 = (int) prectPtr->totalBbox.x2;
    prectPtr->header.y1 = (int) prectPtr->totalBbox.y1;
    prectPtr->header.y2 = (int) prectPtr->totalBbox.y2;
}

/*----------------------------------------------------------------------*/

