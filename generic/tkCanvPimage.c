/*
 * tkCanvPimage.c --
 *
 *	This file implements an image canvas item modelled after its
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


/*
 * The structure below defines the record for each path item.
 */

typedef struct PimageItem  {
    Tk_PathItem header;	    /* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;   /* Canvas containing item. */
    Tk_PathStyle style;	    /* Contains most drawing info. @@@ Only few (two) elements used! */
    Tcl_Obj *styleObj;	    /* Object with style name. */
    double coord[2];	    /* nw coord. */
    Tcl_Obj *imageObj;	    /* Object describing the -image option.
			     * NULL means no image right now. */
    Tk_Image image;	    /* Image to display in window, or NULL if
                             * no image at present. */
    Tk_PhotoHandle photo;
    double width;	    /* If 0 use natural width or height. */
    double height;
    PathRect bbox;	    /* Bounding box with zero width outline.
                             * Untransformed coordinates. */
} PimageItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePimageBbox(Tk_PathCanvas canvas, PimageItem *pimagePtr);
static int	ConfigurePimage(Tcl_Interp *interp, Tk_PathCanvas canvas, 
                        Tk_PathItem *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int	CreatePimage(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void	DeletePimage(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display);
static void	DisplayPimage(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int	PimageCoords(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int	PimageToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static double	PimageToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
static int	PimageToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass);
static void	ScalePimage(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void	TranslatePimage(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double deltaX, double deltaY);
static void	ImageChangedProc _ANSI_ARGS_((ClientData clientData,
                        int x, int y, int width, int height, int imgWidth,
                        int imgHeight));


enum {
    PIMAGE_OPTION_INDEX_FILLOPACITY		    = (1L << (PATH_STYLE_OPTION_INDEX_END + 0)),
    PIMAGE_OPTION_INDEX_HEIGHT			    = (1L << (PATH_STYLE_OPTION_INDEX_END + 1)),
    PIMAGE_OPTION_INDEX_IMAGE			    = (1L << (PATH_STYLE_OPTION_INDEX_END + 2)),
    PIMAGE_OPTION_INDEX_WIDTH			    = (1L << (PATH_STYLE_OPTION_INDEX_END + 3)),
};
 
PATH_STYLE_CUSTOM_OPTION_MATRIX
PATH_CUSTOM_OPTION_TAGS

#define PATH_OPTION_SPEC_FILLOPACITY(typeName)		    \
    {TK_OPTION_DOUBLE, "-fillopacity", NULL, NULL,	    \
        "1.0", -1, Tk_Offset(typeName, style.fillOpacity),  \
	0, 0, PIMAGE_OPTION_INDEX_FILLOPACITY}

#define PATH_OPTION_SPEC_HEIGHT				    \
    {TK_OPTION_DOUBLE, "-height", NULL, NULL,		    \
        "0", -1, Tk_Offset(PimageItem, height),		    \
	0, 0, PIMAGE_OPTION_INDEX_HEIGHT}

#define PATH_OPTION_SPEC_IMAGE				    \
    {TK_OPTION_STRING, "-image", NULL, NULL,		    \
        NULL, Tk_Offset(PimageItem, imageObj), -1,	    \
	TK_OPTION_NULL_OK, 0, PIMAGE_OPTION_INDEX_IMAGE}

#define PATH_OPTION_SPEC_WIDTH				    \
    {TK_OPTION_DOUBLE, "-width", NULL, NULL,		    \
        "0", -1, Tk_Offset(PimageItem, width),		    \
	0, 0, PIMAGE_OPTION_INDEX_WIDTH}

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(PimageItem),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_MATRIX(PimageItem),
    PATH_OPTION_SPEC_FILLOPACITY(PimageItem),
    PATH_OPTION_SPEC_HEIGHT,
    PATH_OPTION_SPEC_IMAGE,
    PATH_OPTION_SPEC_WIDTH,
    PATH_OPTION_SPEC_END
};

static Tk_OptionTable optionTable = NULL;

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPimageType = {
    "pimage",				/* name */
    sizeof(PimageItem),			/* itemSize */
    CreatePimage,			/* createProc */
    optionSpecs,			/* optionSpecs */
    ConfigurePimage,			/* configureProc */
    PimageCoords,			/* coordProc */
    DeletePimage,			/* deleteProc */
    DisplayPimage,			/* displayProc */
    0,					/* flags */
    PimageToPoint,			/* pointProc */
    PimageToArea,			/* areaProc */
    PimageToPostscript,			/* postscriptProc */
    ScalePimage,			/* scaleProc */
    TranslatePimage,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};
                        
 

static int		
CreatePimage(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
     
    TkPathCreateStyle(&(pimagePtr->style));
    pimagePtr->canvas = canvas;
    pimagePtr->styleObj = NULL;
    pimagePtr->imageObj = NULL;
    pimagePtr->image = NULL;
    pimagePtr->photo = NULL;
    pimagePtr->height = 0;
    pimagePtr->width = 0;
    pimagePtr->bbox = NewEmptyPathRect();
    
     if (optionTable == NULL) {
	optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    } 
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) pimagePtr, optionTable, 
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }    
    if (PimageCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePimage(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    DeletePimage(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PimageCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    int result;

	result = CoordsForPointItems(interp, canvas, pimagePtr->coord, objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 2))) {
        ComputePimageBbox(canvas, pimagePtr);
    }
    return result;
}

void
ComputePimageBbox(Tk_PathCanvas canvas, PimageItem *pimagePtr)
{
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    Tk_PathState state = pimagePtr->header.state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (pimagePtr->image == NULL) {
        pimagePtr->header.x1 = pimagePtr->header.x2 =
        pimagePtr->header.y1 = pimagePtr->header.y2 = -1;
        return;
    } else {
        int width = 0, height = 0;
        PathRect bbox;
        
        Tk_SizeOfImage(pimagePtr->image, &width, &height);
        if (pimagePtr->width > 0.0) {
            width = (int) (pimagePtr->width + 1.0);
        }
        if (pimagePtr->height > 0.0) {
            height = (int) (pimagePtr->height + 1.0);
        }
        bbox.x1 = pimagePtr->coord[0];
        bbox.y1 = pimagePtr->coord[1];
        bbox.x2 = bbox.x1 + width;
        bbox.y2 = bbox.y1 + height;
        pimagePtr->bbox = bbox;
        SetGenericPathHeaderBbox(&(pimagePtr->header), stylePtr->matrixPtr, &bbox);
    }
}

static int		
ConfigurePimage(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    Tk_Window tkwin;
    Tk_Image image;
    Tk_PhotoHandle photo;
    Tk_SavedOptions savedOptions;
    Tk_PathItem *parentPtr;
    int mask;
    int result;

    tkwin = Tk_PathCanvasTkwin(canvas);
    result = Tk_SetOptions(interp, (char *) pimagePtr, optionTable, 
	    objc, objv, tkwin, &savedOptions, &mask);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if ((pimagePtr->styleObj != NULL) && (mask & PATH_CORE_OPTION_STYLENAME)) {
        result = TkPathCanvasStyleMergeStyles(tkwin, canvas, stylePtr, pimagePtr->styleObj, 0);
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

    /*
     * Create the image.  Save the old image around and don't free it
     * until after the new one is allocated.  This keeps the reference
     * count from going to zero so the image doesn't have to be recreated
     * if it hasn't changed.
     */
    
    if (mask & PIMAGE_OPTION_INDEX_IMAGE) {
	if (pimagePtr->imageObj != NULL) {
	    image = Tk_GetImage(interp, tkwin, 
		    Tcl_GetString(pimagePtr->imageObj),
		    ImageChangedProc, (ClientData) pimagePtr);
	    if (image == NULL) {
		return TCL_ERROR;
	    }
	    photo = Tk_FindPhoto(interp, Tcl_GetString(pimagePtr->imageObj));
	    if (photo == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    image = NULL;
	    photo = NULL;
	}
	if (pimagePtr->image != NULL) {
	    Tk_FreeImage(pimagePtr->image);
	}
	pimagePtr->image = image;
	pimagePtr->photo = photo;
    }
    
    /*
     * Recompute bounding box for image.
     */
    ComputePimageBbox(canvas, pimagePtr);
    return TCL_OK;
}

static void		
DeletePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    if (pimagePtr->image != NULL) {
        Tk_FreeImage(pimagePtr->image);
    }
    Tk_FreeConfigOptions((char *) pimagePtr, optionTable, Tk_PathCanvasTkwin(canvas));
}

static void		
DisplayPimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    TkPathContext ctx;

    ctx = TkPathInit(Tk_PathCanvasTkwin(canvas), drawable);
    TkPathPushTMatrix(ctx, &m);
    if (stylePtr->matrixPtr != NULL) {
        TkPathPushTMatrix(ctx, stylePtr->matrixPtr);
    }
    /* @@@ Maybe we should taking care of x, y etc.? */
    TkPathImage(ctx, pimagePtr->image, pimagePtr->photo, pimagePtr->coord[0], pimagePtr->coord[1], 
            pimagePtr->width, pimagePtr->height);
    TkPathFree(ctx);
}

static double	
PimageToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    return PathRectToPointWithMatrix(pimagePtr->bbox, stylePtr->matrixPtr, pointPtr);
}

static int		
PimageToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    return PathRectToAreaWithMatrix(pimagePtr->bbox, stylePtr->matrixPtr, areaPtr);
}

static int		
PimageToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{
    /* Skip? */
}

static void		
TranslatePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double deltaX, double deltaY)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pimagePtr->style);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(pimagePtr->bbox), deltaX, deltaY);
    pimagePtr->coord[0] += deltaX;
    pimagePtr->coord[1] += deltaY;
    SetGenericPathHeaderBbox(&(pimagePtr->header), stylePtr->matrixPtr, &(pimagePtr->bbox));
}

static void
ImageChangedProc(
    ClientData clientData,	/* Pointer to canvas item for image. */
    int x, int y,		/* Upper left pixel (within image)
                                 * that must be redisplayed. */
    int width, int height,	/* Dimensions of area to redisplay
                                 * (may be <= 0). */
    int imgWidth, int imgHeight)/* New dimensions of image. */
{
    PimageItem *pimagePtr = (PimageItem *) clientData;

    /*
     * If the image's size changed and it's not anchored at its
     * northwest corner then just redisplay the entire area of the
     * image.  This is a bit over-conservative, but we need to do
     * something because a size change also means a position change.
     */
     
    /* @@@ MUST consider our own width and height settings as well and TMatrix. */

    if (((pimagePtr->header.x2 - pimagePtr->header.x1) != imgWidth)
            || ((pimagePtr->header.y2 - pimagePtr->header.y1) != imgHeight)) {
        x = y = 0;
        width = imgWidth;
        height = imgHeight;
        Tk_PathCanvasEventuallyRedraw(pimagePtr->canvas, pimagePtr->header.x1,
                pimagePtr->header.y1, pimagePtr->header.x2, pimagePtr->header.y2);
    } 
    ComputePimageBbox(pimagePtr->canvas, pimagePtr);
    Tk_PathCanvasEventuallyRedraw(pimagePtr->canvas, pimagePtr->header.x1 + x,
            pimagePtr->header.y1 + y, (int) (pimagePtr->header.x1 + x + width),
            (int) (pimagePtr->header.y1 + y + height));
}

/*----------------------------------------------------------------------*/

