/*
 * path.c --
 *
 *	This file is main for the tkpath package.
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2007  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* Keep patch level release numbers odd and set even only on release. */
#define TKPATH_VERSION    "0.2"
#define TKPATH_PATCHLEVEL "0.2.1"

extern Tk_ItemType tkPathType;
extern Tk_ItemType tkPrectType;
extern Tk_ItemType tkPlineType;
extern Tk_ItemType tkPolylineType;
extern Tk_ItemType tkPpolygonType;
extern Tk_ItemType tkCircleType;
extern Tk_ItemType tkEllipseType;
extern Tk_ItemType tkPimageType;
extern Tk_ItemType tkPtextType;

int gUseAntiAlias = 0;
Tcl_Interp *gInterp = NULL;

extern int 	LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* CONST objv[]);
extern int 	RadialGradientCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* CONST objv[]);
extern int 	PixelAlignObjCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* CONST objv[]);


#ifdef _WIN32
    BOOL APIENTRY
    DllMain( hInst, reason, reserved )
        HINSTANCE   hInst;		/* Library instance handle. */
        DWORD       reason;		/* Reason this function is being called. */
        LPVOID      reserved;	/* Not used. */
    {
        return TRUE;
    }
#endif


/*
 *----------------------------------------------------------------------
 *
 * Tkpath_Init --
 *
 *		Initializer for the tkpath package.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#ifdef _WIN32
    __declspec(dllexport)
#endif

int Tkpath_Init(Tcl_Interp *interp)		/* Tcl interpreter. */
{
        
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	    return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.1", 0) == NULL) {
		return TCL_ERROR;
    }
#endif

    gInterp = interp;
	
    Tk_CreateItemType(&tkPathType);
    Tk_CreateItemType(&tkPrectType);
    Tk_CreateItemType(&tkPlineType);
    Tk_CreateItemType(&tkPolylineType);
    Tk_CreateItemType(&tkPpolygonType);
    Tk_CreateItemType(&tkCircleType);
    Tk_CreateItemType(&tkEllipseType);
    Tk_CreateItemType(&tkPimageType);
    Tk_CreateItemType(&tkPtextType);
    
    /*
     * Link the ::tkpath::antialias variable to control antialiasing. 
     */
    Tcl_EvalEx(interp, "namespace eval ::tkpath {}", -1, TCL_EVAL_GLOBAL);
    if (Tcl_LinkVar(interp, "::tkpath::antialias",
            (char *) &gUseAntiAlias, TCL_LINK_BOOLEAN) != TCL_OK) {
        Tcl_ResetResult(interp);
    }
    
    Tcl_CreateObjCommand(interp, "::tkpath::pixelalign",
            PixelAlignObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    /*
     * Make separate gradient objects, similar to SVG.
     */
    Tcl_CreateObjCommand(interp, "::tkpath::lineargradient",
            LinearGradientCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "::tkpath::radialgradient",
            RadialGradientCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    PathGradientInit(interp);

    /*
     * Style object.
     */
    Tcl_CreateObjCommand(interp, "::tkpath::style",
            StyleObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    PathStyleInit(interp);

    return Tcl_PkgProvide(interp, "tkpath", TKPATH_PATCHLEVEL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tkpath_SafeInit --
 *
 *		This is just to provide a "safe" entry point (that is not safe!).
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#ifdef _WIN32
    __declspec(dllexport)
#endif

int Tkpath_SafeInit(Tcl_Interp *interp)
{
    return Tkpath_Init(interp);
}

/* This is for the tk based drawing backend. */
#ifdef _WIN32
    __declspec(dllexport)
#endif

int Tkpathtk_Init(Tcl_Interp *interp)
{
    return Tkpath_Init(interp);
}

#ifdef _WIN32
    __declspec(dllexport)
#endif

int Tkpathtk_SafeInit(Tcl_Interp *interp)
{
    return Tkpath_SafeInit(interp);
}

/*
 * On Windows we've got two different libs for GDI and GDI+ named differently.
 */
#ifdef _WIN32
    __declspec(dllexport)

int Tkpathgdi_Init(Tcl_Interp *interp)
{
    return Tkpath_Init(interp);
}

    __declspec(dllexport)

int Tkpathgdi_SafeInit(Tcl_Interp *interp)
{
    return Tkpath_SafeInit(interp);
}

    __declspec(dllexport)

int Tkpathgdiplus_SafeInit(Tcl_Interp *interp)
{
    return Tkpath_SafeInit(interp);
}

    __declspec(dllexport)

int Tkpathgdiplus_Init(Tcl_Interp *interp)
{
    return Tkpath_Init(interp);
}
#endif

/*--------------------------------------------------------------------------------*/
