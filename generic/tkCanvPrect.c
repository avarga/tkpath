/*
 * tkCanvPrect.c --
 *
 *	This file implements a rectangle canvas item modelled after its
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
    kPrectItemNoNewPathAtoms      	= (1L << 0)	/* Inhibit any 'MakePathAtoms' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PrectItem  {
    Tk_PathItem header;	    /* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;   /* Canvas containing item. */
    Tk_PathStyle style;	    /* Contains most drawing info. */
    Tcl_Obj *styleObj;	    /* Object with style name. */
    TkPathStyleInst *styleInst;
			    /* The referenced style instance from styleObj. */
    double rx;		    /* Radius of corners. */
    double ry;
    PathRect rect;	    /* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    PathRect totalBbox;	    /* Bounding box including stroke.
                             * Untransformed coordinates. */
    int maxNumSegments;	    /* Max number of straight segments (for subpath)
                             * needed for Area and Point functions. */
    long flags;		    /* Various flags, see enum. */
    char *null;		    /* Just a placeholder for not yet implemented stuff. */ 
} PrectItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePrectBbox(Tk_PathCanvas canvas, PrectItem *prectPtr);
static int	ConfigurePrect(Tcl_Interp *interp, Tk_PathCanvas canvas, 
                        Tk_PathItem *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int	CreatePrect(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void	DeletePrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int	PrectCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	PrectToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static double	PrectToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
static int	PrectToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass);
static void	ScalePrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void	TranslatePrect(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double deltaX, double deltaY);
static PathAtom * MakePathAtoms(PrectItem *prectPtr);
static void	PrectGradientProc(ClientData clientData, int flags);
static void	PrectStyleProc(ClientData clientData, int flags);


enum {
    PRECT_OPTION_INDEX_RX   = (1L << (PATH_STYLE_OPTION_INDEX_END + 0)),
    PRECT_OPTION_INDEX_RY   = (1L << (PATH_STYLE_OPTION_INDEX_END + 1)),
};
 
PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS

#define PATH_OPTION_SPEC_RX(typeName)		    \
    {TK_OPTION_DOUBLE, "-rx", NULL, NULL,	    \
        "0.0", -1, Tk_Offset(typeName, rx),	    \
	0, 0, PRECT_OPTION_INDEX_RX}

#define PATH_OPTION_SPEC_RY(typeName)		    \
    {TK_OPTION_DOUBLE, "-ry", NULL, NULL,	    \
        "0.0", -1, Tk_Offset(typeName, ry),	    \
	0, 0, PRECT_OPTION_INDEX_RY}

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(PrectItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(PrectItem, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(PrectItem),
    PATH_OPTION_SPEC_STYLE_STROKE(PrectItem, "black"),
    PATH_OPTION_SPEC_RX(PrectItem),
    PATH_OPTION_SPEC_RY(PrectItem),
    PATH_OPTION_SPEC_END
};

static Tk_OptionTable optionTable = NULL;

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPrectType = {
    "prect",				/* name */
    sizeof(PrectItem),			/* itemSize */
    CreatePrect,			/* createProc */
    optionSpecs,			/* optionSpecs OBSOLTE !!! ??? */
    ConfigurePrect,			/* configureProc */
    PrectCoords,			/* coordProc */
    DeletePrect,			/* deleteProc */
    DisplayPrect,			/* displayProc */
    0,					/* flags */
    PrectToPoint,			/* pointProc */
    PrectToArea,			/* areaProc */
    PrectToPostscript,			/* postscriptProc */
    ScalePrect,				/* scaleProc */
    TranslatePrect,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};
                        

static int		
CreatePrect(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathCreateStyle(&(prectPtr->style));
    prectPtr->canvas = canvas;
    prectPtr->styleObj = NULL;
    prectPtr->styleInst = NULL;
    prectPtr->rect = NewEmptyPathRect();
    prectPtr->totalBbox = NewEmptyPathRect();
    prectPtr->maxNumSegments = 100;		/* Crude overestimate. */
    prectPtr->flags = 0L;
    
    if (optionTable == NULL) {
	optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    } 
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) prectPtr, optionTable, 
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
    DeletePrect(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PrectCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    int result;

    result = CoordsForRectangularItems(interp, canvas, &(prectPtr->rect), objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 4))) {
        if (!(prectPtr->flags & kPrectItemNoNewPathAtoms)) {
            ComputePrectBbox(canvas, prectPtr);
        }
    }
    return result;
}

void
ComputePrectBbox(Tk_PathCanvas canvas, PrectItem *prectPtr)
{
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    Tk_PathState state = prectPtr->header.state;

    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (state == TK_PATHSTATE_HIDDEN) {
        prectPtr->header.x1 = prectPtr->header.x2 =
        prectPtr->header.y1 = prectPtr->header.y2 = -1;
        return;
    }
    prectPtr->totalBbox = GetGenericPathTotalBboxFromBare(NULL, stylePtr, &(prectPtr->rect));
    SetGenericPathHeaderBbox(&(prectPtr->header), stylePtr->matrixPtr, &(prectPtr->totalBbox));
}

static int		
ConfigurePrect(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    Tk_PathItem *parentPtr;
    Tk_Window tkwin;
    Tk_PathState state;
    Tk_SavedOptions savedOptions;
    int mask;
    Tcl_Obj *errorResult = NULL;
    int error;
    
    // @@@ NB: We could try to make something much more generic with these options !!!
    //         There is a lot of code duplicated for each path type item...
    // ItemGenericConfigure(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    //	    int objc, Tcl_Obj *CONST objv[], Tk_PathStyle *stylePtr,
    //	    TkPathGradientChangedProc *changeGradProc, 
    //	    TkPathStyleChangedProc *changeStyleProc);
    // See the Tk_PathItemEx record. Then perhaps:
    // ItemGenericConfigure(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItemEx *itemPtr,
    //	    int objc, Tcl_Obj *CONST objv[]);
    //
    // Tk_PathItemEx *itemExPtr = (Tk_PathItemEx *) itemPtr;

     
    tkwin = Tk_PathCanvasTkwin(canvas);
    for (error = 0; error <= 1; error++) {
	if (!error) {
	    if (Tk_SetOptions(interp, (char *) prectPtr, optionTable, 
		    objc, objv, tkwin, &savedOptions, &mask) != TCL_OK) {
		continue;
	    }
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);
	}
	if (mask & PATH_CORE_OPTION_PARENT) {
	    if (TkPathCanvasFindGroup(interp, canvas, itemPtr->parentObj, &parentPtr) != TCL_OK) {
		continue;
	    }
	    TkPathCanvasSetParent(parentPtr, itemPtr);
	}

	/*
	 * If we have got a style name it's options take precedence
	 * over the actual path configuration options. This is how SVG does it.
	 * Good or bad?
	 */
	if (mask & PATH_CORE_OPTION_STYLENAME) {
	    TkPathStyleInst *styleInst = NULL;
	    	
	    if (prectPtr->styleObj != NULL) {
		styleInst = TkPathGetStyle(interp, Tcl_GetString(prectPtr->styleObj),
			TkPathCanvasStyleTable(canvas), PrectStyleProc,
			(ClientData) itemPtr);
		if (styleInst == NULL) {
		    continue;
		}
	    } else {
		styleInst = NULL;
	    }
	    if (prectPtr->styleInst != NULL) {
		TkPathFreeStyle(prectPtr->styleInst);
	    }
	    prectPtr->styleInst = styleInst;    
	} 
        
	/*
	 * Just translate the 'fillObj' (string) to a TkPathColor.
	 * We MUST have this last in the chain of custom option checks!
	 */
	if (mask & PATH_STYLE_OPTION_FILL) {
	    TkPathColor *fillPtr = NULL;
	    
	    if (stylePtr->fillObj != NULL) {
		fillPtr = TkPathGetPathColor(interp, tkwin, stylePtr->fillObj,
			TkPathCanvasGradientTable(canvas), PrectGradientProc,
			(ClientData) itemPtr);
		if (fillPtr == NULL) {
		    continue;
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
	
	/*
	 * If we reach this on the first pass we are OK and continue below.
	 */
	break;
    }
    if (!error) {
	Tk_FreeSavedOptions(&savedOptions);
    }
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    stylePtr->fillOpacity   = MAX(0.0, MIN(1.0, stylePtr->fillOpacity));
    prectPtr->rx = MAX(0.0, prectPtr->rx);
    prectPtr->ry = MAX(0.0, prectPtr->ry);
    
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
    ComputePrectBbox(canvas, prectPtr);
    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
	return TCL_OK;
    }
}

static PathAtom *
MakePathAtoms(PrectItem *prectPtr)
{
    PathAtom *atomPtr;
    double points[4];
    
    points[0] = prectPtr->rect.x1;
    points[1] = prectPtr->rect.y1;
    points[2] = prectPtr->rect.x2;
    points[3] = prectPtr->rect.y2;
    TkPathMakePrectAtoms(points, prectPtr->rx, prectPtr->ry, &atomPtr);
    return atomPtr;
}

static void		
DeletePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    if (prectPtr->styleInst != NULL) {
	TkPathFreeStyle(prectPtr->styleInst);
    }
    Tk_FreeConfigOptions((char *) itemPtr, optionTable, Tk_PathCanvasTkwin(canvas));
}

static void		
DisplayPrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    PathAtom *atomPtr;
            
    Tk_PathStyle style = prectPtr->style;
    if (prectPtr->styleInst != NULL) {
	TkPathStyleMergeStyles(prectPtr->styleInst->masterPtr, &style, 0);
    }
    
    atomPtr = MakePathAtoms(prectPtr);
    TkPathDrawPath(Tk_PathCanvasTkwin(canvas), drawable, atomPtr, 
	    &style, &m, &(prectPtr->rect));
    TkPathFreeAtoms(atomPtr);
}

static double	
PrectToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    PathRect *rectPtr = &(prectPtr->rect);
    double bareRect[4];
    double width, dist;
    int rectiLinear = 0;
    int filled;

    filled = HaveAnyFillFromPathColor(stylePtr->fill);
    width = 0.0;
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
    }
    
    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = rectPtr->x1;
            bareRect[1] = rectPtr->y1;
            bareRect[2] = rectPtr->x2;
            bareRect[3] = rectPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
        
            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * rectPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * rectPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * rectPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * rectPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        dist = PathRectToPoint(bareRect, width, filled, pointPtr);
    } else {
	PathAtom *atomPtr = MakePathAtoms(prectPtr);
        dist = GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            prectPtr->maxNumSegments, pointPtr);
	TkPathFreeAtoms(atomPtr);
    }
    return dist;
}

static int		
PrectToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    PathRect *rectPtr = &(prectPtr->rect);
    double bareRect[4];
    double width;
    int rectiLinear = 0;
    int filled;

    filled = HaveAnyFillFromPathColor(stylePtr->fill);
    width = 0.0;
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
    }

    /* Try to be economical about this for pure rectangles. */
    if ((prectPtr->rx <= 1.0) && (prectPtr->ry <= 1.0)) {
        if (mPtr == NULL) {
            rectiLinear = 1;
            bareRect[0] = rectPtr->x1;
            bareRect[1] = rectPtr->y1;
            bareRect[2] = rectPtr->x2;
            bareRect[3] = rectPtr->y2;
        } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
        
            /* This is a situation we can treat in a simplified way. Apply the transform here. */
            rectiLinear = 1;
            bareRect[0] = mPtr->a * rectPtr->x1 + mPtr->tx;
            bareRect[1] = mPtr->d * rectPtr->y1 + mPtr->ty;
            bareRect[2] = mPtr->a * rectPtr->x2 + mPtr->tx;
            bareRect[3] = mPtr->d * rectPtr->y2 + mPtr->ty;
        }
    }
    if (rectiLinear) {
        return PathRectToArea(bareRect, width, filled, areaPtr);
    } else {
	int area;
	PathAtom *atomPtr = MakePathAtoms(prectPtr);
        area = GenericPathToArea(canvas, itemPtr, &(prectPtr->style), 
                atomPtr, prectPtr->maxNumSegments, areaPtr);
	TkPathFreeAtoms(atomPtr);
	return area;
    }
}

static int		
PrectToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslatePrect(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double deltaX, double deltaY)
{
    PrectItem *prectPtr = (PrectItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(prectPtr->style);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(prectPtr->rect), deltaX, deltaY);
    TranslatePathRect(&(prectPtr->totalBbox), deltaX, deltaY);
    SetGenericPathHeaderBbox(&(prectPtr->header), stylePtr->matrixPtr, &(prectPtr->totalBbox));
}

// @@@ TODO: both these two could be made generically by using Tk_PathItemEx.

static void	
PrectGradientProc(ClientData clientData, int flags)
{
    PrectItem *prectPtr = (PrectItem *)clientData;
    Tk_PathStyle *stylePtr = &(prectPtr->style);
        
    if (flags) {
	if (flags & PATH_GRADIENT_FLAG_DELETE) {
	    TkPathFreePathColor(stylePtr->fill);	
	    stylePtr->fill = NULL;
	    Tcl_DecrRefCount(stylePtr->fillObj);
	    stylePtr->fillObj = NULL;
	}
	Tk_PathCanvasEventuallyRedraw(prectPtr->canvas,
		prectPtr->header.x1, prectPtr->header.y1,
		prectPtr->header.x2, prectPtr->header.y2);
    }
}

static void	
PrectStyleProc(ClientData clientData, int flags)
{
    PrectItem *prectPtr = (PrectItem *)clientData;
        
    if (flags) {
	if (flags & PATH_STYLE_FLAG_DELETE) {
	    TkPathFreeStyle(prectPtr->styleInst);	
	    prectPtr->styleInst = NULL;
	    Tcl_DecrRefCount(prectPtr->styleObj);
	    prectPtr->styleObj = NULL;
	}
	Tk_PathCanvasEventuallyRedraw(prectPtr->canvas,
		prectPtr->header.x1, prectPtr->header.y1,
		prectPtr->header.x2, prectPtr->header.y2);
    }
}

/*----------------------------------------------------------------------*/

