/*
 * path.c --
 *
 *	This file is main for the tkpath package.
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <tcl.h>
#include <tk.h>
#include "tkIntPath.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define TKPATH_VERSION "0.1"

extern Tk_ItemType tkPathType;
int gUseAntialiasing = 0;

/*
 * Hash table to keep track of linear gradient fills.
 */    
    
Tcl_HashTable 	*gLinearGradientHashPtr = NULL;

extern int 	LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
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

int 
Tkpath_Init(
    Tcl_Interp *interp)		/* Tcl interpreter. */
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
	
    Tk_CreateItemType(&tkPathType);
    
    /*
     * Link the ::tkpath::antialias variable to control antialiasing. 
     */
    Tcl_EvalEx(interp, "namespace eval ::tkpath {}", -1, TCL_EVAL_GLOBAL);
    if (Tcl_LinkVar(interp, "::tkpath::antialias",
            (char *) &gUseAntialiasing, TCL_LINK_BOOLEAN) != TCL_OK) {
        Tcl_ResetResult(interp);
    }

    /*
     * Make a separate gradient object, similar to SVG.
     */
    Tcl_CreateObjCommand(interp, "::tkpath::lineargradient",
            LinearGradientCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    PathLinearGradientInit(interp);
    /*
    gLinearGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable(gLinearGradientHashPtr, TCL_STRING_KEYS);
*/
    Tcl_CreateObjCommand(interp, "::tkpath::style",
            StyleObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    PathStyleInit(interp);

    return Tcl_PkgProvide(interp, "tkpath", TKPATH_VERSION);
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

int 
Tkpath_SafeInit(
    Tcl_Interp *interp)		/* Tcl interpreter. */
{
    return Tkpath_Init(interp);
}

/*--------------------------------------------------------------------------------*/
