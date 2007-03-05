/*
 * tkCanvEllipse.c --
 *
 *	This file implements the circle and ellipse canvas items modelled after its
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
 * The structure below defines the record for each circle and ellipse item.
 */

typedef struct EllipseItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    char type;				/* Circle or ellipse. */
    double center[2];		/* Center coord. */
    double rx;				/* Radius. Circle uses rx for overall radius. */
    double ry;
} EllipseItem;

enum {
    kOvalTypeCircle,
    kOvalTypeEllipse
};

/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputeEllipseBbox(Tk_Canvas canvas, EllipseItem *ellPtr);
static int		ConfigureEllipse(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreateAny(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[], char type);
static int		CreateCircle(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
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


PATH_STYLE_CUSTOM_CONFIG_RECORDS

static Tk_ConfigSpec configSpecsCircle[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(EllipseItem, ""),
    PATH_CONFIG_SPEC_STYLE_MATRIX(EllipseItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(EllipseItem, "black"),
    PATH_CONFIG_SPEC_CORE(EllipseItem),
    
    {TK_CONFIG_DOUBLE, "-r", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(EllipseItem, rx), 0, 0},

    PATH_END_CONFIG_SPEC
};

/* @@@ Better way? */
#define PELLIPSE_OPTION_INDEX_R 	(sizeof(configSpecsCircle)/sizeof(Tk_ConfigSpec) - 2)

static Tk_ConfigSpec configSpecsEllipse[] = {
    PATH_CONFIG_SPEC_STYLE_FILL(EllipseItem, ""),
    PATH_CONFIG_SPEC_STYLE_MATRIX(EllipseItem),
    PATH_CONFIG_SPEC_STYLE_STROKE(EllipseItem, "black"),
    PATH_CONFIG_SPEC_CORE(EllipseItem),
    
    {TK_CONFIG_DOUBLE, "-rx", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(EllipseItem, rx), 0, 0},
    {TK_CONFIG_DOUBLE, "-ry", (char *) NULL, (char *) NULL,
        "0.0", Tk_Offset(EllipseItem, ry), 0, 0},

    PATH_END_CONFIG_SPEC
};

/* @@@ Better way? */
#define PELLIPSE_OPTION_INDEX_RX 	(sizeof(configSpecsEllipse)/sizeof(Tk_ConfigSpec) - 3)
#define PELLIPSE_OPTION_INDEX_RY 	(sizeof(configSpecsEllipse)/sizeof(Tk_ConfigSpec) - 2)
 
/*
 * The structures below define the 'circle' and 'ellipse' item types by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkCircleType = {
    "circle",						/* name */
    sizeof(EllipseItem),			/* itemSize */
    CreateCircle,					/* createProc */
    configSpecsCircle,				/* configSpecs */
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

Tk_ItemType tkEllipseType = {
    "ellipse",						/* name */
    sizeof(EllipseItem),			/* itemSize */
    CreateEllipse,					/* createProc */
    configSpecsEllipse,				/* configSpecs */
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
CreateCircle(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kOvalTypeCircle);
}

static int		
CreateEllipse(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kOvalTypeEllipse);
}

static int		
CreateAny(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[], char type)
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
    TkPathCreateStyle(&(ellPtr->style));
    ellPtr->canvas = canvas;
    ellPtr->type = type;
    ellPtr->styleName = NULL;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    if (EllipseCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigureEllipse(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
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

	result = CoordsForPointItems(interp, canvas, ellPtr->center, objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 2))) {
        ComputeEllipseBbox(canvas, ellPtr);
    }
    return result;
}

static PathRect
GetBareBbox(EllipseItem *ellPtr)
{
    PathRect bbox;
    
    bbox.x1 = ellPtr->center[0] - ellPtr->rx;
    bbox.y1 = ellPtr->center[1] - ellPtr->ry;
    bbox.x2 = ellPtr->center[0] + ellPtr->rx;
    bbox.y2 = ellPtr->center[1] + ellPtr->ry;
    return bbox;
}

static void
ComputeEllipseBbox(Tk_Canvas canvas, EllipseItem *ellPtr)
{
    Tk_PathStyle *stylePtr = &(ellPtr->style);
    Tk_State state = ellPtr->header.state;
    PathRect totalBbox, bbox;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }
    if (state == TK_STATE_HIDDEN) {
        ellPtr->header.x1 = ellPtr->header.x2 =
        ellPtr->header.y1 = ellPtr->header.y2 = -1;
        return;
    }
    bbox = GetBareBbox(ellPtr);
    totalBbox = GetGenericPathTotalBboxFromBare(NULL, stylePtr, &bbox);
    SetGenericPathHeaderBbox(&(ellPtr->header), stylePtr->matrixPtr, &totalBbox);
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
    int result = TCL_OK;

    tkwin = Tk_CanvasTkwin(canvas);
    if (ellPtr->type == kOvalTypeCircle) {
        result = Tk_ConfigureWidget(interp, tkwin, configSpecsCircle, objc,
                (CONST char **) objv, (char *) ellPtr, flags|TK_CONFIG_OBJS);
    } else {
        result = Tk_ConfigureWidget(interp, tkwin, configSpecsEllipse, objc,
                (CONST char **) objv, (char *) ellPtr, flags|TK_CONFIG_OBJS);
    }	
    if (result != TCL_OK) {
        return TCL_ERROR;
    }
    
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));
    stylePtr->fillOpacity   = MAX(0.0, MIN(1.0, stylePtr->fillOpacity));
    ellPtr->rx = MAX(0.0, ellPtr->rx);
    ellPtr->ry = MAX(0.0, ellPtr->ry);
    if (ellPtr->type == kOvalTypeCircle) {
        /* Practical. */
        ellPtr->ry = ellPtr->rx;
    }
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
    if ((configSpecsEllipse[PELLIPSE_OPTION_INDEX_RX].specFlags & TK_CONFIG_OPTION_SPECIFIED) ||
            (configSpecsEllipse[PELLIPSE_OPTION_INDEX_RY].specFlags & TK_CONFIG_OPTION_SPECIFIED)) {

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
DeleteEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    /* Empty? */
}

static void		
DisplayEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    PathRect bbox;
    PathAtom *atomPtr;
    EllipseAtom ellAtom;
    
    /* 
     * We create the atom on the fly to save some memory.
     */    
    atomPtr = (PathAtom *)&ellAtom;
    atomPtr->nextPtr = NULL;
    atomPtr->type = PATH_ATOM_ELLIPSE;
    ellAtom.cx = ellPtr->center[0];
    ellAtom.cy = ellPtr->center[1];
    ellAtom.rx = ellPtr->rx;
    ellAtom.ry = ellPtr->ry;
    
    bbox = GetBareBbox(ellPtr);
    TkPathDrawPath(Tk_CanvasTkwin(canvas), drawable, atomPtr, &(ellPtr->style), &m, &bbox);
}

static double	
EllipseToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ellPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    double bareOval[4];
    double width, dist;
    int rectiLinear = 0;
    int haveDist = 0;
    int filled;

    filled = GetColorFromPathColor(stylePtr->fill) != NULL;
    width = 0.0;
    if (stylePtr->strokeColor != NULL) {
        width = stylePtr->strokeWidth;
    }
    if (mPtr == NULL) {
        rectiLinear = 1;
        bareOval[0] = ellPtr->center[0] - ellPtr->rx;
        bareOval[1] = ellPtr->center[1] - ellPtr->ry;
        bareOval[2] = ellPtr->center[0] + ellPtr->rx;
        bareOval[3] = ellPtr->center[1] + ellPtr->ry;
        
        /* For tiny points make it simple. */
        if ((ellPtr->rx <= 2.0) && (ellPtr->ry <= 2.0)) {
            dist = hypot(ellPtr->center[0] - pointPtr[0], ellPtr->center[1] - pointPtr[1]);
            dist = MAX(0.0, dist - (ellPtr->rx + ellPtr->ry)/2.0);
            haveDist = 1;
        }
    } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
        double rx, ry;
    
        /* This is a situation we can treat in a simplified way. Apply the transform here. */
        rectiLinear = 1;
        bareOval[0] = mPtr->a * (ellPtr->center[0] - ellPtr->rx) + mPtr->tx;
        bareOval[1] = mPtr->d * (ellPtr->center[1] - ellPtr->ry) + mPtr->ty;
        bareOval[2] = mPtr->a * (ellPtr->center[0] + ellPtr->rx) + mPtr->tx;
        bareOval[3] = mPtr->d * (ellPtr->center[1] + ellPtr->ry) + mPtr->ty;

        /* For tiny points make it simple. */
        rx = fabs(bareOval[0] - bareOval[2])/2.0;
        ry = fabs(bareOval[1] - bareOval[3])/2.0;
        if ((rx <= 2.0) && (ry <= 2.0)) {
            dist = hypot((bareOval[0] + bareOval[2]/2.0) - pointPtr[0], 
                    (bareOval[1] + bareOval[3]/2.0) - pointPtr[1]);
            dist = MAX(0.0, dist - (rx + ry)/2.0);
            haveDist = 1;
        }
    }
    if (!haveDist) {
        if (rectiLinear) {
            dist = TkOvalToPoint(bareOval, width, filled, pointPtr);
        } else {
            PathAtom *atomPtr;
            EllipseAtom ellAtom;
        
            /* 
            * We create the atom on the fly to save some memory.
            */    
            atomPtr = (PathAtom *)&ellAtom;
            atomPtr->nextPtr = NULL;
            atomPtr->type = PATH_ATOM_ELLIPSE;
            ellAtom.cx = ellPtr->center[0];
            ellAtom.cy = ellPtr->center[1];
            ellAtom.rx = ellPtr->rx;
            ellAtom.ry = ellPtr->ry;
            dist = GenericPathToPoint(canvas, itemPtr, stylePtr, atomPtr, 
                    kPathNumSegmentsEllipse+1, pointPtr);
        }
    }
    return dist;
}

static int		
EllipseToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(ellPtr->style);
    TMatrix *mPtr = stylePtr->matrixPtr;
    double bareOval[4], halfWidth;
    int rectiLinear = 0;
    int result;
    
    halfWidth = 0.0;
    if (stylePtr->strokeColor != NULL) {
        halfWidth = stylePtr->strokeWidth/2.0;
    }
    if (mPtr == NULL) {
        rectiLinear = 1;
        bareOval[0] = ellPtr->center[0] - ellPtr->rx;
        bareOval[1] = ellPtr->center[1] - ellPtr->ry;
        bareOval[2] = ellPtr->center[0] + ellPtr->rx;
        bareOval[3] = ellPtr->center[1] + ellPtr->ry;
    } else if (TMATRIX_IS_RECTILINEAR(mPtr)) {
    
        /* This is a situation we can treat in a simplified way. Apply the transform here. */
        rectiLinear = 1;
        bareOval[0] = mPtr->a * (ellPtr->center[0] - ellPtr->rx) + mPtr->tx;
        bareOval[1] = mPtr->d * (ellPtr->center[1] - ellPtr->ry) + mPtr->ty;
        bareOval[2] = mPtr->a * (ellPtr->center[0] + ellPtr->rx) + mPtr->tx;
        bareOval[3] = mPtr->d * (ellPtr->center[1] + ellPtr->ry) + mPtr->ty;
    }
    
    if (rectiLinear) {
        double oval[4];
        
        /* @@@ Assuming untransformed strokes */
        oval[0] = bareOval[0] - halfWidth;
        oval[1] = bareOval[1] - halfWidth;
        oval[2] = bareOval[2] + halfWidth;
        oval[3] = bareOval[3] + halfWidth;

        result = TkOvalToArea(oval, areaPtr);
    
        /*
         * If the rectangle appears to overlap the oval and the oval
         * isn't filled, do one more check to see if perhaps all four
         * of the rectangle's corners are totally inside the oval's
         * unfilled center, in which case we should return "outside".
         */
        if ((result == 0) && (stylePtr->strokeColor != NULL)
                && (GetColorFromPathColor(stylePtr->fill) != NULL)) {
            double width, height;
            double xDelta1, yDelta1, xDelta2, yDelta2;
        
            width = (bareOval[2] - bareOval[0])/2.0 - halfWidth;
            height = (bareOval[3] - bareOval[1])/2.0 - halfWidth;
            if ((width <= 0.0) || (height <= 0.0)) {
                return 0;
            }
            xDelta1 = (areaPtr[0] - ellPtr->center[0])/width;
            xDelta1 *= xDelta1;
            yDelta1 = (areaPtr[1] - ellPtr->center[1])/height;
            yDelta1 *= yDelta1;
            xDelta2 = (areaPtr[2] - ellPtr->center[0])/width;
            xDelta2 *= xDelta2;
            yDelta2 = (areaPtr[3] - ellPtr->center[1])/height;
            yDelta2 *= yDelta2;
            if (((xDelta1 + yDelta1) < 1.0)
                    && ((xDelta1 + yDelta2) < 1.0)
                    && ((xDelta2 + yDelta1) < 1.0)
                    && ((xDelta2 + yDelta2) < 1.0)) {
                return -1;
            }
        }
    } else {
        PathAtom *atomPtr;
        EllipseAtom ellAtom;
    
        /* 
         * We create the atom on the fly to save some memory.
         */    
        atomPtr = (PathAtom *)&ellAtom;
        atomPtr->nextPtr = NULL;
        atomPtr->type = PATH_ATOM_ELLIPSE;
        ellAtom.cx = ellPtr->center[0];
        ellAtom.cy = ellPtr->center[1];
        ellAtom.rx = ellPtr->rx;
        ellAtom.ry = ellPtr->ry;
        return GenericPathToArea(canvas, itemPtr, stylePtr, atomPtr, 
                kPathNumSegmentsEllipse+1, areaPtr);
    }
    return result;
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
}

static void		
TranslateEllipse(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    EllipseItem *ellPtr = (EllipseItem *) itemPtr;

    /* Just translate the bbox'es as well. */
    ellPtr->center[0] += deltaX;
    ellPtr->center[1] += deltaY;
    /* Beware for cumlated round-off errors! */
    ellPtr->header.x1 += (int) deltaX;
    ellPtr->header.x2 += (int) deltaX;
    ellPtr->header.y1 += (int) deltaY;
    ellPtr->header.y2 += (int) deltaY;
}

/*----------------------------------------------------------------------*/

