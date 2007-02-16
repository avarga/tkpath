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


typedef struct PathSurface {
    TkPathContext ctx;
	char *token;
    int width;
    int height;
} PathSurface;

static Tcl_HashTable 	*surfaceHashPtr = NULL;

static int 	NewSurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceDestroyObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr);
static void	SurfaceDeletedProc(ClientData clientData);
static int 	SurfaceCreateObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]);
static int 	SurfaceEraseObjCmd(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]);

static int	SurfaceCreatePath(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]) ;

static int	uid = 0;
static char *kSurfaceNameBase = "tkpath::surface";


static CONST char *surfaceCmds[] = {
    "copy", "create", "destroy", "erase", "height", "width",
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
    "path", "prect",
    (char *) NULL
};

enum {
	kPathSurfaceItemPath						= 0L,
    kPathSurfaceItemPrect
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
        case kPathSurfaceItemPath: {
            result = SurfaceCreatePath(interp, surfacePtr, objc, objv);
            break;
        }
        case kPathSurfaceItemPrect: {

            break;
        }
    }
    return result;
}

static int
SurfaceCreatePath(Tcl_Interp* interp, PathSurface *surfacePtr, int objc, Tcl_Obj* CONST objv[]) 
{
    TkPathContext 	context = surfacePtr->ctx;
    Tk_PathStyle 	style;
    PathAtom 		*atomPtr = NULL;
    PathRect		bbox;
    int				len;
    int 			result = TCL_OK;

    TkPathCreateStyle(&style);
    if (TCL_OK != TkPathParseToAtoms(interp, objv[3], &atomPtr, &len)) {
        return TCL_ERROR;
    }
    if (TCL_OK != TkPathConfigStyle(interp, &style, objc-4, objv+4)) {
        return TCL_ERROR;
    }
    if (TkPathMakePath(context, atomPtr, &style) != TCL_OK) {
        return TCL_ERROR;
    }
    bbox = TkPathGetTotalBbox(atomPtr, &style);
    TkPathPaintPath(context, atomPtr, &style, &bbox);
    TkPathFree(context);
    TkPathDeleteStyle(Tk_Display(Tk_MainWindow(interp)), &style);
    TkPathFreeAtoms(atomPtr);
    return result;
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

