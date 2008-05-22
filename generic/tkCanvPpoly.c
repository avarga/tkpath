/*
 * tkCanvPpolygon.c --
 *
 *	This file implements polygon and polyline canvas items modelled after its
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

/* Values for the PpolyItem's flag. */

enum {
    kPpolyItemNoBboxCalculation     	= (1L << 0)		/* Inhibit any 'ComputePpolyBbox' call. */
};

/*
 * The structure below defines the record for each path item.
 */

typedef struct PpolyItem  {
    Tk_PathItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;	/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    Tcl_Obj *styleObj;		/* Object with style name. */
    char type;			/* Polyline or polygon. */
    PathAtom *atomPtr;
    PathRect bbox;		/* Bounding box with zero width outline.
				 * Untransformed coordinates. */
    PathRect totalBbox;		/* Bounding box including stroke.
				 * Untransformed coordinates. */
    int maxNumSegments;		/* Max number of straight segments (for subpath)
				 * needed for Area and Point functions. */
    long flags;			/* Various flags, see enum. */
    char *null;   		/* Just a placeholder for not yet implemented stuff. */ 
} PpolyItem;

enum {
    kPpolyTypePolyline,
    kPpolyTypePolygon
};


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePpolyBbox(Tk_PathCanvas canvas, PpolyItem *ppolyPtr);
static int	ConfigurePpoly(Tcl_Interp *interp, Tk_PathCanvas canvas, 
                        Tk_PathItem *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
int		CoordsForPolygonline(Tcl_Interp *interp, Tk_PathCanvas canvas, int closed,
                        int objc, Tcl_Obj *CONST objv[], PathAtom **atomPtrPtr, int *lenPtr);
static int	CreateAny(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[], char type);
static int	CreatePolyline(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	CreatePpolygon(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void	DeletePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display);
static void	DisplayPpoly(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int	PpolyCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	PpolyToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static double	PpolyToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
static int	PpolyToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass);
static void	ScalePpoly(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void	TranslatePpoly(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double deltaX, double deltaY);
static void	PpolyGradientProc(ClientData clientData, int flags);


PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS

static Tk_OptionSpec optionSpecsPolyline[] = {
    PATH_OPTION_SPEC_CORE(PpolyItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(PpolyItem, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(PpolyItem),
    PATH_OPTION_SPEC_STYLE_STROKE(PpolyItem, "black"),
    PATH_OPTION_SPEC_END
};

static Tk_OptionSpec optionSpecsPpolygon[] = {
    PATH_OPTION_SPEC_CORE(PpolyItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(PpolyItem, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(PpolyItem),
    PATH_OPTION_SPEC_STYLE_STROKE(PpolyItem, "black"),
    PATH_OPTION_SPEC_END
};

static Tk_OptionTable optionTablePolyline = NULL;
static Tk_OptionTable optionTablePpolygon = NULL;

/*
 * The structures below defines the 'polyline' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPolylineType = {
    "polyline",				/* name */
    sizeof(PpolyItem),			/* itemSize */
    CreatePolyline,			/* createProc */
    optionSpecsPolyline,		/* OptionSpecs */
    ConfigurePpoly,			/* configureProc */
    PpolyCoords,			/* coordProc */
    DeletePpoly,			/* deleteProc */
    DisplayPpoly,			/* displayProc */
    0,					/* flags */
    PpolyToPoint,			/* pointProc */
    PpolyToArea,			/* areaProc */
    PpolyToPostscript,			/* postscriptProc */
    ScalePpoly,				/* scaleProc */
    TranslatePpoly,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};

Tk_PathItemType tkPpolygonType = {
    "ppolygon",				/* name */
    sizeof(PpolyItem),			/* itemSize */
    CreatePpolygon,			/* createProc */
    optionSpecsPpolygon,		/* OptionSpecs */
    ConfigurePpoly,			/* configureProc */
    PpolyCoords,			/* coordProc */
    DeletePpoly,			/* deleteProc */
    DisplayPpoly,			/* displayProc */
    0,					/* flags */
    PpolyToPoint,			/* pointProc */
    PpolyToArea,			/* areaProc */
    PpolyToPostscript,			/* postscriptProc */
    ScalePpoly,				/* scaleProc */
    TranslatePpoly,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};
 

static int		
CreatePolyline(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kPpolyTypePolyline);
}

static int		
CreatePpolygon(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kPpolyTypePolygon);
}

static int		
CreateAny(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[], char type)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_OptionTable optionTable;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathCreateStyle(&(ppolyPtr->style));
    ppolyPtr->canvas = canvas;
    ppolyPtr->styleObj = NULL;
    ppolyPtr->atomPtr = NULL;
    ppolyPtr->type = type;
    ppolyPtr->bbox = NewEmptyPathRect();
    ppolyPtr->totalBbox = NewEmptyPathRect();
    ppolyPtr->maxNumSegments = 0;
    ppolyPtr->flags = 0L;
    
    if (ppolyPtr->type == kPpolyTypePolyline) {
	if (optionTablePolyline == NULL) {
	    optionTablePolyline = Tk_CreateOptionTable(interp, optionSpecsPolyline);
	}
	optionTable = optionTablePolyline;
    } else {
	if (optionTablePpolygon == NULL) {
	    optionTablePpolygon = Tk_CreateOptionTable(interp, optionSpecsPpolygon);
	}
	optionTable = optionTablePpolygon;    
    }
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) ppolyPtr, optionTable, 
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
     * Since both PpolyCoords and ConfigurePpoly computes new bbox'es
     * we skip this and do it ourself below.
     */
    ppolyPtr->flags |= kPpolyItemNoBboxCalculation;
    if (PpolyCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePpoly(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        ppolyPtr->flags &= ~kPpolyItemNoBboxCalculation;
        ComputePpolyBbox(canvas, ppolyPtr);
        return TCL_OK;
    }

    error:
    DeletePpoly(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PpolyCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    int len, closed;

    closed = (ppolyPtr->type == kPpolyTypePolyline) ? 0 : 1;
    if (CoordsForPolygonline(interp, canvas, closed, objc, objv, 
            &(ppolyPtr->atomPtr), &len) != TCL_OK) {
        return TCL_ERROR;
    }
    ppolyPtr->maxNumSegments = len;
    if (!(ppolyPtr->flags & kPpolyItemNoBboxCalculation)) {
        ComputePpolyBbox(canvas, ppolyPtr);
    }
    return TCL_OK;
}	

void
ComputePpolyBbox(Tk_PathCanvas canvas, PpolyItem *ppolyPtr)
{
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);
    Tk_PathState state = ppolyPtr->header.state;

    if(state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (ppolyPtr->atomPtr == NULL || (state == TK_PATHSTATE_HIDDEN)) {
        ppolyPtr->header.x1 = ppolyPtr->header.x2 =
        ppolyPtr->header.y1 = ppolyPtr->header.y2 = -1;
        return;
    }
    ppolyPtr->bbox = GetGenericBarePathBbox(ppolyPtr->atomPtr);
    ppolyPtr->totalBbox = GetGenericPathTotalBboxFromBare(ppolyPtr->atomPtr,
            stylePtr, &(ppolyPtr->bbox));
    SetGenericPathHeaderBbox(&(ppolyPtr->header), stylePtr->matrixPtr, &(ppolyPtr->totalBbox));
}

static int		
ConfigurePpoly(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);
    Tk_Window tkwin;
    Tk_PathState state;
    Tk_SavedOptions savedOptions;
    Tk_PathItem *parentPtr;
    int mask;
    int result = TCL_OK;

    tkwin = Tk_PathCanvasTkwin(canvas);
    if (ppolyPtr->type == kPpolyTypePolyline) {
	result = Tk_SetOptions(interp, (char *) ppolyPtr, optionTablePolyline, 
		objc, objv, tkwin, &savedOptions, &mask);
    } else {
	result = Tk_SetOptions(interp, (char *) ppolyPtr, optionTablePpolygon, 
		objc, objv, tkwin, &savedOptions, &mask);
    }	
    if (result != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if ((ppolyPtr->styleObj != NULL) && (mask & PATH_CORE_OPTION_STYLENAME)) {
        result = TkPathCanvasStyleMergeStyles(tkwin, canvas, stylePtr, ppolyPtr->styleObj, 0);
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
		    TkPathCanvasGradientTable(canvas), PpolyGradientProc,
		    (ClientData) itemPtr);
	    if (fillPtr == NULL) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	} else {
	    fillPtr = NULL;
	}
	if (stylePtr->fill != NULL) {
	    TkPathFreePathColor(stylePtr->fill);
	}
	stylePtr->fill = fillPtr;
    }
    Tk_FreeSavedOptions(&savedOptions);
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    
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
    if (!(ppolyPtr->flags & kPpolyItemNoBboxCalculation)) {
        ComputePpolyBbox(canvas, ppolyPtr);
    }
    return TCL_OK;
}

static void		
DeletePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_OptionTable optionTable;
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    if (ppolyPtr->atomPtr != NULL) {
        TkPathFreeAtoms(ppolyPtr->atomPtr);
        ppolyPtr->atomPtr = NULL;
    }
    optionTable = (ppolyPtr->type == kPpolyTypePolyline) ? optionTablePolyline : optionTablePpolygon;
    Tk_FreeConfigOptions((char *) itemPtr, optionTable, Tk_PathCanvasTkwin(canvas));
}

static void		
DisplayPpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);

    TkPathDrawPath(Tk_PathCanvasTkwin(canvas), drawable, ppolyPtr->atomPtr, &(ppolyPtr->style),
            &m, &(ppolyPtr->bbox));
}

static double	
PpolyToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    PathAtom *atomPtr = ppolyPtr->atomPtr;
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);

    return GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
            ppolyPtr->maxNumSegments, pointPtr);
}

static int		
PpolyToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    
    return GenericPathToArea(canvas, itemPtr, &(ppolyPtr->style), 
            ppolyPtr->atomPtr, ppolyPtr->maxNumSegments, areaPtr);
}

static int		
PpolyToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* This doesn't work very well with general affine matrix transforms! Arcs ? */
    //     ScalePathAtoms(atomPtr, originX, originY, scaleX, scaleY);
}

static void		
TranslatePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double deltaX, double deltaY)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    PathAtom *atomPtr = ppolyPtr->atomPtr;
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);

    TranslatePathAtoms(atomPtr, deltaX, deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(ppolyPtr->bbox), deltaX, deltaY);
    TranslatePathRect(&(ppolyPtr->totalBbox), deltaX, deltaY);
    SetGenericPathHeaderBbox(&(ppolyPtr->header), stylePtr->matrixPtr, &(ppolyPtr->totalBbox));
}

/*
 *--------------------------------------------------------------
 *
 * CoordsForPolygonline --
 *
 *		Used as coordProc for polyline and polygon items.
 *
 * Results:
 *		Standard tcl result.
 *
 * Side effects:
 *		May store new atoms in atomPtrPtr and max number of points
 *		in lenPtr.
 *
 *--------------------------------------------------------------
 */

int		
CoordsForPolygonline(
    Tcl_Interp *interp, 
    Tk_PathCanvas canvas, 
    int closed,				/* Polyline (0) or polygon (1) */
    int objc, 
    Tcl_Obj *CONST objv[],
    PathAtom **atomPtrPtr,
    int *lenPtr)
{
    PathAtom *atomPtr = *atomPtrPtr;

    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
        
        while (atomPtr != NULL) {
            switch (atomPtr->type) {
                case PATH_ATOM_M: { 
                    MoveToAtom *move = (MoveToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->y));
                    break;
                }
                case PATH_ATOM_L: {
                    LineToAtom *line = (LineToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->y));
                    break;
                }
                case PATH_ATOM_Z: {
                
                    break;
                }
                default: {
                    /* empty */
                }
            }
            atomPtr = atomPtr->nextPtr;
        }
        Tcl_SetObjResult(interp, obj);
        return TCL_OK;
    }
    if (objc == 1) {
        if (Tcl_ListObjGetElements(interp, objv[0], &objc,
            (Tcl_Obj ***) &objv) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    if (objc & 1) {
        char buf[64 + TCL_INTEGER_SPACE];
        sprintf(buf, "wrong # coordinates: expected an even number, got %d", objc);
        Tcl_SetResult(interp, buf, TCL_VOLATILE);
        return TCL_ERROR;
    } else if (objc < 4) {
        char buf[64 + TCL_INTEGER_SPACE];
        sprintf(buf, "wrong # coordinates: expected at least 4, got %d", objc);
        Tcl_SetResult(interp, buf, TCL_VOLATILE);
        return TCL_ERROR;
    } else {
        int 	i;
        double	x, y;
        double	firstX = 0.0, firstY = 0.0;
        PathAtom *firstAtomPtr = NULL;
    
        /*
        * Free any old stuff.
        */
        if (atomPtr != NULL) {
            TkPathFreeAtoms(atomPtr);
            atomPtr = NULL;
        }
        for (i = 0; i < objc; i += 2) {
            if (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[i], &x) != TCL_OK) {
                /* @@@ error recovery? */
                return TCL_ERROR;
            }
            if (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[i+1], &y) != TCL_OK) {
                return TCL_ERROR;
            }
            if (i == 0) {
                firstX = x;
                firstY = y;
                atomPtr = NewMoveToAtom(x, y);
                firstAtomPtr = atomPtr;
            } else {
                atomPtr->nextPtr = NewLineToAtom(x, y);
                atomPtr = atomPtr->nextPtr;
            }
        }
        if (closed) {
            atomPtr->nextPtr = NewCloseAtom(firstX, firstY);
        }
        *atomPtrPtr = firstAtomPtr;
        *lenPtr = i/2 + 2;
    }
    return TCL_OK;
}

static void	
PpolyGradientProc(ClientData clientData, int flags)
{
    PpolyItem *ppolyPtr = (PpolyItem *)clientData;
    Tk_PathStyle *stylePtr = &(ppolyPtr->style);
    
    if (flags) {
	if (flags & PATH_GRADIENT_FLAG_DELETE) {
	    TkPathFreePathColor(stylePtr->fill);	
	    stylePtr->fill = NULL;
	    Tcl_DecrRefCount(stylePtr->fillObj);
	    stylePtr->fillObj = NULL;
	}
	Tk_PathCanvasEventuallyRedraw(ppolyPtr->canvas,
		ppolyPtr->header.x1, ppolyPtr->header.y1,
		ppolyPtr->header.x2, ppolyPtr->header.y2);
    }
}

/*----------------------------------------------------------------------*/

