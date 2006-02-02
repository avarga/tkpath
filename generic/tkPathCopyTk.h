/*
 * Code picked up from various places in the Tk sources.
 *
 * $Id$
 *
 */
 
#ifndef INCLUDED_TKPATHCOPYTK_H
#define INCLUDED_TKPATHCOPYTK_H

#include <tcl.h>
#include <tk.h>
#include <tkInt.h>
#include "tkPort.h"
#include "tkInt.h"
#include "tkCanvas.h"
#include "tkPath.h"
#include "tkIntPath.h"

#ifdef __cplusplus
extern "C" {
#endif

int		PathTk_CanvasTagsParseProc(ClientData clientData,
                Tcl_Interp *interp, Tk_Window tkwin,
                CONST char *value, char *recordPtr, int offset);
char *	PathTk_CanvasTagsPrintProc(ClientData clientData, Tk_Window tkwin, 
                char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		PathTkCanvasDashParseProc(ClientData clientData, Tcl_Interp *interp,
                Tk_Window tkwin, CONST char *value,
                char *widgRec, int offset);

char *	PathTkCanvasDashPrintProc(ClientData clientData,
                Tk_Window tkwin, char *widgRec, int offset,
                Tcl_FreeProc **freeProcPtr);
int		PathTkPixelParseProc(ClientData clientData, Tcl_Interp *interp,
                Tk_Window tkwin, CONST char *value,
                char *widgRec, int offset);
char *	PathTkPixelPrintProc(ClientData clientData, Tk_Window tkwin,    
                char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		PathTkStateParseProc(ClientData clientData, Tcl_Interp *interp,
                Tk_Window tkwin, CONST char *value,    
                char *widgRec, int offset);
char *	PathTkStatePrintProc(ClientData clientData, Tk_Window tkwin,
                char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		PathTkOffsetParseProc(ClientData clientData, Tcl_Interp *interp,
                Tk_Window tkwin, CONST char *value,    
                char *widgRec, int offset);
char *	PathTkOffsetPrintProc(ClientData clientData, Tk_Window tkwin,
                char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
    

int 		LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                int objc, Tcl_Obj* CONST objv[]);
void		PathPaintLinearGradientFromName(TkPathContext ctx, PathRect *bbox, char *name, int fillRule);


#ifdef __cplusplus
}
#endif

#endif      // INCLUDED_TKPATHCOPYTK_H
