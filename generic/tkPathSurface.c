/*
 * tkPathSurface.c --
 *
 *	    This file implements style objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkPathStyle.h"


typedef struct PathSurface {
    TkPathContext ctx;
	char *token;
    int width;
    int height;
} PathSurface;

static Tcl_HashTable 	*surfaceHashPtr = NULL;

static int 	NewSurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceCopyObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceDestroyObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr);
static void	SurfaceDeletedProc(ClientData clientData);
static int 	SurfaceCreateObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceEraseObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]);

static int	SurfaceCreateCircle(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]) ;
static int	SurfaceCreatePath(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]) ;
static void	SurfaceInitOptions(Tcl_Interp* interp);

static int	uid = 0;
static char *kSurfaceNameBase = "tkpath::surface";

static CONST char *surfaceCmds[] = {
    "copy", 	"create", 	"destroy", 
    "erase", 	"height", 	"width",
    (char *) NULL
};

enum {
	kPathSurfaceCmdCopy						= 0L,
    kPathSurfaceCmdCreate,
    kPathSurfaceCmdDestroy,
    kPathSurfaceCmdErase,
    kPathSurfaceCmdHeight,
    kPathSurfaceCmdWidth
};

int
InitSurface(Tcl_Interp *interp)
{
    surfaceHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable(surfaceHashPtr, TCL_STRING_KEYS);

    Tcl_CreateObjCommand(interp, "::tkpath::surface",
            NewSurfaceObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    SurfaceInitOptions(interp);
    return TCL_OK;
}

static int 
NewSurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[])
{
    TkPathContext ctx;
    PathSurface *surfacePtr;
    Tcl_HashEntry *hPtr;
    char str[255];
    int width, height;
    int isNew;
    int result = TCL_OK;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "width height");
        return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[1], &width) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[2], &height) != TCL_OK) {
        return TCL_ERROR;
    }
    
    ctx = TkPathInitSurface(width, height);
    if (ctx == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("Failed in TkPathInitSurface", -1));
        return TCL_ERROR;
    }

    sprintf(str, "%s%d", kSurfaceNameBase, uid++);
    surfacePtr = (PathSurface *) ckalloc( sizeof(PathSurface) );
    surfacePtr->token = ckalloc( strlen(str) + 1 );
    strcpy(surfacePtr->token, str);
    surfacePtr->ctx = ctx;
    surfacePtr->width = width;
    surfacePtr->height = height;
    Tcl_CreateObjCommand(interp, str, SurfaceObjCmd, (ClientData) surfacePtr, SurfaceDeletedProc);

    hPtr = Tcl_CreateHashEntry(surfaceHashPtr, str, &isNew);
    Tcl_SetHashValue(hPtr, surfacePtr);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(str, -1));
    return result;
}

static int 
SurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[])
{
    PathSurface *surfacePtr = (PathSurface *) clientData;
    int 		index;
    int 		result = TCL_OK;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg...?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], surfaceCmds, "command", 0,
            &index) != TCL_OK) {
        return TCL_ERROR;
    }

    switch (index) {
        case kPathSurfaceCmdCopy: {
            result = SurfaceCopyObjCmd(interp, surfacePtr, objc, objv);
            break;
        }
        case kPathSurfaceCmdCreate: {
            result = SurfaceCreateObjCmd(interp, surfacePtr, objc, objv);
            break;
        }
        case kPathSurfaceCmdDestroy: {
            result = SurfaceDestroyObjCmd(interp, surfacePtr);
            break;
        }
        case kPathSurfaceCmdErase: {
            result = SurfaceEraseObjCmd(interp, surfacePtr, objc, objv);
            break;
        }
        case kPathSurfaceCmdHeight: {
            if (objc != 2) {
                Tcl_WrongNumArgs(interp, 2, objv, NULL);
                return TCL_ERROR;
            }
            Tcl_SetObjResult(interp, Tcl_NewIntObj(surfacePtr->height));
            break;
        }
        case kPathSurfaceCmdWidth: {
            if (objc != 2) {
                Tcl_WrongNumArgs(interp, 2, objv, NULL);
                return TCL_ERROR;
            }
            Tcl_SetObjResult(interp, Tcl_NewIntObj(surfacePtr->width));
            break;
        }
    }
    
    return result;
}

static int 
SurfaceCopyObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[])
{
    Tk_PhotoHandle photo;
    
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "image");
        return TCL_ERROR;
    }
    photo = Tk_FindPhoto( interp, Tcl_GetString(objv[2]) );
    if (photo == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("didn't find that image", -1));
        return TCL_ERROR;
    }
    TkPathSurfaceToPhoto(surfacePtr->ctx, photo);
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_OK;
}

static int 
SurfaceDestroyObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr)
{
    Tcl_DeleteCommand(interp, surfacePtr->token);
    return TCL_OK;
}

static void
SurfaceDeletedProc(ClientData clientData)
{
    PathSurface *surfacePtr = (PathSurface *) clientData;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(surfaceHashPtr, surfacePtr->token);
    if (hPtr != NULL) {
        Tcl_DeleteHashEntry(hPtr);
    }
    TkPathFree(surfacePtr->ctx);
    ckfree(surfacePtr->token);
    ckfree((char *)surfacePtr);
}

static CONST char *surfaceItemCmds[] = {
    "circle",    "ellipse",  "path", 
    "pimage",    "pline",    "polyline", 
    "ppolygon",  "prect",    "ptext",
    (char *) NULL
};

enum {
	kPathSurfaceItemCircle				= 0L,
    kPathSurfaceItemEllipse,
    kPathSurfaceItemPath,
    kPathSurfaceItemPimage,
    kPathSurfaceItemPline,
    kPathSurfaceItemPolyline,
    kPathSurfaceItemPpolygon,
    kPathSurfaceItemPrect,
    kPathSurfaceItemPtext
};

static int 
SurfaceCreateObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[])
{
    int 		index;
    int 		result = TCL_OK;

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "type ?arg arg...?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], surfaceItemCmds, "type", 0,
            &index) != TCL_OK) {
        return TCL_ERROR;
    }

    switch (index) {
        case kPathSurfaceItemCircle: {
            result = SurfaceCreateCircle(interp, surfacePtr, objc, objv);
            break;
        }
        case kPathSurfaceItemEllipse: {

            break;
        case kPathSurfaceItemPath: {
            result = SurfaceCreatePath(interp, surfacePtr, objc, objv);
            break;
        }
        }
        case kPathSurfaceItemPimage: {

            break;
        }
        case kPathSurfaceItemPline: {

            break;
        }
        case kPathSurfaceItemPolyline: {

            break;
        }
        case kPathSurfaceItemPpolygon: {

            break;
        }
        case kPathSurfaceItemPrect: {

            break;
        }
        case kPathSurfaceItemPtext: {

            break;
        }
    }
    return result;
}

static Tk_OptionTable 	gOptionTableCircle;
static Tk_OptionTable 	gOptionTableEllipse;
static Tk_OptionTable 	gOptionTablePath;
static Tk_OptionTable 	gOptionTablePimage;
static Tk_OptionTable 	gOptionTablePline;
static Tk_OptionTable 	gOptionTablePolyline;
static Tk_OptionTable 	gOptionTablePpolygon;
static Tk_OptionTable 	gOptionTablePrect;
static Tk_OptionTable 	gOptionTablePtext;


PATH_STYLE_CUSTOM_OPTION_RECORDS

#define PATH_OPTION_SPEC_STYLENAME(typeName)							\
    {TK_OPTION_STRING, "-style", (char *) NULL, (char *) NULL,  		\
        "", -1, Tk_Offset(typeName, styleName), TK_OPTION_NULL_OK, 0, 0}

#define PATH_OPTION_SPEC_R(typeName)									\
    {TK_OPTION_DOUBLE, "-r", (char *) NULL, (char *) NULL,  			\
        "0.0", -1, Tk_Offset(typeName, r), 0, 0, 0}

#define PATH_OPTION_SPEC_RX(typeName)									\
    {TK_OPTION_DOUBLE, "-rx", (char *) NULL, (char *) NULL,  			\
        "0.0", -1, Tk_Offset(typeName, rx), 0, 0, 0}

#define PATH_OPTION_SPEC_RY(typeName)									\
    {TK_OPTION_DOUBLE, "-ry", (char *) NULL, (char *) NULL,  			\
        "0.0", -1, Tk_Offset(typeName, ry), 0, 0, 0}


static int	
GetPointCoords(Tcl_Interp *interp, double *pointPtr, int objc, Tcl_Obj *CONST objv[])
{
    if ((objc == 1) || (objc == 2)) {
        double x, y;
        
        if (objc==1) {
            if (Tcl_ListObjGetElements(interp, objv[0], &objc,
                    (Tcl_Obj ***) &objv) != TCL_OK) {
                return TCL_ERROR;
            } else if (objc != 4) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 2", -1));
                return TCL_ERROR;
            }
        }
        if ((Tcl_GetDoubleFromObj(interp, objv[0], &x) != TCL_OK)
            || (Tcl_GetDoubleFromObj(interp, objv[1], &y) != TCL_OK)) {
            return TCL_ERROR;
        }
        pointPtr[0] = x;
        pointPtr[1] = y;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # coordinates: expected 2", -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int
GetFirstOptionIndex(int objc, Tcl_Obj* CONST objv[])
{
    int i;
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    return i;
}

static int
SurfaceParseOptions(Tcl_Interp *interp, char *recordPtr, 
        Tk_OptionTable table, int objc, Tcl_Obj* CONST objv[])
{
    Tk_Window tkwin = Tk_MainWindow(interp);    
    if (Tk_InitOptions(interp, recordPtr, table, tkwin) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tk_SetOptions(interp, recordPtr, table, 	
            objc, objv, tkwin, NULL, NULL) != TCL_OK) {
        Tk_FreeConfigOptions(recordPtr, table, NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

typedef struct SurfCircleItem {
    char *styleName;
    Tk_PathStyle style;
    double r;
} SurfCircleItem;

static Tk_OptionSpec circleOptionSpecs[] = {
    PATH_OPTION_SPEC_STYLENAME(SurfCircleItem),
    PATH_OPTION_SPEC_STYLE_FILL(SurfCircleItem, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(SurfCircleItem),
    PATH_OPTION_SPEC_STYLE_STROKE(SurfCircleItem, "black"),
    PATH_OPTION_SPEC_R(SurfCircleItem),
    PATH_OPTION_SPEC_END
};

static int	
SurfaceCreateCircle(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[])
{
    TkPathContext 	context = surfacePtr->ctx;
    int				i;
    double			center[2];
    double			rx, ry;
    PathAtom 		*atomPtr;
    EllipseAtom 	ellAtom;
    PathRect		bbox;
    SurfCircleItem  circle;

    circle.styleName = NULL;
    i = GetFirstOptionIndex(objc, objv);
    TkPathCreateStyle(&(circle.style));
	if (GetPointCoords(interp, center, i-3, objv+3) != TCL_OK) {
        return TCL_ERROR;
    }
    if (SurfaceParseOptions(interp, (char *)&circle, gOptionTableCircle, objc-i, objv+i) != TCL_OK) {
        return TCL_ERROR;
    }
    if (circle.styleName != NULL) {
        PathStyleMergeStyles(Tk_MainWindow(interp), &(circle.style), circle.styleName, 0);
    } 
    atomPtr = (PathAtom *)&ellAtom;
    atomPtr->nextPtr = NULL;
    atomPtr->type = PATH_ATOM_ELLIPSE;
    ellAtom.cx = center[0];
    ellAtom.cy = center[1];
    ellAtom.rx = circle.r;
    ellAtom.ry = circle.r;
    
    bbox = TkPathGetTotalBbox(atomPtr, &(circle.style));
    TkPathPaintPath(context, atomPtr, &(circle.style), &bbox);
    TkPathEndPath(context);
    TkPathDeleteStyle(Tk_Display(Tk_MainWindow(interp)), &(circle.style));
    return TCL_OK;
}

typedef struct SurfPathItem {
    char *styleName;
    Tk_PathStyle style;
} SurfPathItem;

static Tk_OptionSpec pathOptionSpecs[] = {
    PATH_OPTION_SPEC_STYLENAME(SurfPathItem),
    PATH_OPTION_SPEC_STYLE_FILL(SurfPathItem, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(SurfPathItem),
    PATH_OPTION_SPEC_STYLE_STROKE(SurfPathItem, "black"),
    PATH_OPTION_SPEC_END
};

static int
SurfaceCreatePath(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]) 
{
    TkPathContext 	context = surfacePtr->ctx;
    PathAtom 		*atomPtr = NULL;
    PathRect		bbox;
    SurfPathItem	path;
    int				len;
    
    path.styleName = NULL;
    TkPathCreateStyle(&(path.style));
    if (TkPathParseToAtoms(interp, objv[3], &atomPtr, &len) != TCL_OK) {
        return TCL_ERROR;
    }
    if (SurfaceParseOptions(interp, (char *)&path, gOptionTablePath, objc-4, objv+4) != TCL_OK) {
        return TCL_ERROR;
    }
    if (path.styleName != NULL) {
        PathStyleMergeStyles(Tk_MainWindow(interp), &(path.style), path.styleName, 0);
    } 
    if (TkPathMakePath(context, atomPtr, &(path.style)) != TCL_OK) {
        return TCL_ERROR;
    }
    bbox = TkPathGetTotalBbox(atomPtr, &(path.style));
    TkPathPaintPath(context, atomPtr, &(path.style), &bbox);
    TkPathEndPath(context);
    TkPathDeleteStyle(Tk_Display(Tk_MainWindow(interp)), &(path.style));
    TkPathFreeAtoms(atomPtr);
    // CRASH!!!
    //Tk_FreeConfigOptions((char *)&path, gOptionTablePath, Tk_MainWindow(interp));
    return TCL_OK;
}

static void
SurfaceInitOptions(Tcl_Interp* interp)
{
    gOptionTableCircle = Tk_CreateOptionTable(interp, circleOptionSpecs);
    //gOptionTableEllipse = Tk_CreateOptionTable(interp, ellipseOptionSpecs);
    gOptionTablePath = Tk_CreateOptionTable(interp, pathOptionSpecs);
    /*
    gOptionTablePimage = Tk_CreateOptionTable(interp, pimageOptionSpecs);
    gOptionTablePline = Tk_CreateOptionTable(interp, plineOptionSpecs);
    gOptionTablePolyline = Tk_CreateOptionTable(interp, polylineOptionSpecs);
    gOptionTablePpolygon = Tk_CreateOptionTable(interp, ppolygonOptionSpecs);
    gOptionTablePrect = Tk_CreateOptionTable(interp, prectOptionSpecs);
    gOptionTablePtext = Tk_CreateOptionTable(interp, ptextOptionSpecs);
    */
}

static int 
SurfaceEraseObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[])
{
    int x, y, width, height;
    
    if (objc != 6) {
        Tcl_WrongNumArgs(interp, 2, objv, "x y width height");
        return TCL_ERROR;
    }
    if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) ||
            (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) ||
            (Tcl_GetIntFromObj(interp, objv[4], &width) != TCL_OK) ||
            (Tcl_GetIntFromObj(interp, objv[5], &height) != TCL_OK)) {
        return TCL_ERROR;
    }
    TkPathSurfaceErase(surfacePtr->ctx, x, y, width, height);
    return TCL_OK;
}

