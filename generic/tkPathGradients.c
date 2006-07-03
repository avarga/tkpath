/*
 * tkPathGradients.c --
 *
 *	    This file implements gradient objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * Note: It would be best to have this in the canvas widget as a special
 *       object, but I see no way of doing this without touching
 *       the canvas code.
 *
 * Note: When a gradient object is modified or destroyed the corresponding
 *       items are not notified. They will only notice any change when
 *       they need to redisplay.
 *
 * $Id$
 */

#include "tkIntPath.h"

/*
 * Hash table to keep track of linear gradient fills.
 */    

Tcl_HashTable 	*gLinearGradientHashPtr = NULL;
Tcl_HashTable 	*gRadialGradientHashPtr = NULL;

static Tk_OptionTable 	gLinearGradientOptionTable;
static Tk_OptionTable 	gRadialGradientOptionTable;
static int 				gLinearGradientNameUid = 0;
static int 				gRadialGradientNameUid = 0;
static char *			kLinearGradientNameBase = "lineargradient";
static char *			kRadialGradientNameBase = "radialgradient";

typedef struct LinearGradientStyle {
	Tk_OptionTable optionTable;
	Tk_Uid name;
    Tcl_Obj *transObj;
    Tcl_Obj *stopsObj;
    LinearGradientFill fill;
} LinearGradientStyle;

typedef struct RadialGradientStyle {
	Tk_OptionTable optionTable;
	Tk_Uid name;
    Tcl_Obj *transObj;
    Tcl_Obj *stopsObj;
    RadialGradientFill fill;
} RadialGradientStyle;

int 				LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* CONST objv[]);
static char *		LinGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]);
static void			LinGradientFree(Tcl_Interp *interp, char *recordPtr);

int 				RadialGradientCmd(ClientData clientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* CONST objv[]);
static char *		RadGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]);
static void			RadGradientFree(Tcl_Interp *interp, char *recordPtr);

/*
 * Custom option processing code.
 */

static char *
ComputeSlotAddress(recordPtr, offset)
    char *recordPtr;	/* Pointer to the start of a record. */
    int offset;		/* Offset of a slot within that record; may be < 0. */
{
    if (offset >= 0) {
        return recordPtr + offset;
    } else {
        return NULL;
    }
}

/*
 * Procedures for processing the transition option of the linear gradient fill.
 * Boundaries are from 0.0 to 1.0.
 */
 
static int LinTransitionSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    char *internalPtr;
    int objEmpty = 0;
    double z[4] = {0.0, 0.0, 1.0, 0.0};
    PathRect *rectPtr = NULL;
    
    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    if (internalPtr != NULL) {
        rectPtr = (PathRect *) internalPtr;
    }
    objEmpty = ObjectIsEmpty((*value));
    
    /*
     * Important: the new value for the transition is not yet 
     * stored into the style! transObj may be NULL!
     * The new value is stored in style *after* we return TCL_OK.
     */
    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        (*value) = NULL;
    } else {
        int i, len;
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, (*value), &len, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        if (len != 4) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(
                    "-lineartransition must have four elements", -1));
            return TCL_ERROR;
        }
        for (i = 0; i < 4; i++) {
            if (Tcl_GetDoubleFromObj(interp, objv[i], z+i) != TCL_OK) {
                return TCL_ERROR;
            }
            if ((z[i] < 0.0) || (z[i] > 1.0)) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "-lineartransition elements must be in the range 0.0 to 1.0", -1));
                return TCL_ERROR;
            }
        }
    }
    if (rectPtr != NULL) {
        rectPtr->x1 = z[0];
        rectPtr->y1 = z[1];
        rectPtr->x2 = z[2];
        rectPtr->y2 = z[3];
    }
    return TCL_OK;
}

static Tk_ObjCustomOption linTransitionCO = 
{
    "lineartransition",
    LinTransitionSet,
    NULL,
    NULL,
    NULL,
    (ClientData) NULL
};

static int RadTransitionSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    char *internalPtr;
    int objEmpty = 0;
    double z[5] = {0.0, 0.0, 1.0, 0.0, 0.0};
    RadialGradientFill *fillPtr = NULL;

    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    if (internalPtr != NULL) {
        fillPtr = (RadialGradientFill *) internalPtr;
    }
    objEmpty = ObjectIsEmpty((*value));
    
    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        (*value) = NULL;
    } else {
        int i, len;
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, (*value), &len, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        if ((len == 1) || (len == 4) || (len > 5)) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(
                    "-radialtransition must be a list {cx cy ?r? ?fx fy?}", -1));
            return TCL_ERROR;
        }
        for (i = 0; i < len; i++) {
            if (Tcl_GetDoubleFromObj(interp, objv[i], z+i) != TCL_OK) {
                return TCL_ERROR;
            }
            if ((z[i] < 0.0) || (z[i] > 1.0)) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "-radialtransition elements must be in the range 0.0 to 1.0", -1));
                return TCL_ERROR;
            }
        }
    }
    if (fillPtr != NULL) {
        fillPtr->centerX = z[0];
        fillPtr->centerY = z[1];
        fillPtr->rad = z[2];
        fillPtr->focalX = z[3];
        fillPtr->focalY = z[4];
    }
    return TCL_OK;
}

static Tk_ObjCustomOption radTransitionCO = 
{
    "radialtransition",
    RadTransitionSet,
    NULL,
    NULL,
    NULL,
    (ClientData) NULL
};

static GradientStop *
NewGradientStop(double offset, XColor *color, double opacity)
{
    GradientStop *stopPtr;
    
	stopPtr = (GradientStop *) ckalloc(sizeof(GradientStop));
	memset(stopPtr, '\0', sizeof(GradientStop));
    stopPtr->offset = offset;
    stopPtr->color = color;
    stopPtr->opacity = opacity;
    return stopPtr;
}

static void
FreeAllStops(GradientStop **stops, int nstops)
{
    int i;

    for (i = 0; i < nstops; i++) {
        if (stops[i] != NULL) {
            ckfree((char *) (stops[i]));
        }
    }
    ckfree((char *) stops);
}

/*
 * The stops are a list of stop lists where each stop list is:
 *		{offset color ?opacity?}
 */
static int StopsSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,		/* Here NULL */
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    char *internalPtr;
    int i, nstops, stopLen;
    int objEmpty = 0;
    double offset, lastOffset, opacity;
    Tcl_Obj **objv;
    Tcl_Obj *stopObj;
    Tcl_Obj *obj;
    XColor *color;
    GradientStop **stops = NULL;
    GradientStopArray *stopArrPtr = NULL;
    
    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    if (internalPtr != NULL) {
        stopArrPtr = (GradientStopArray *)internalPtr;
    }
    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        (*value) = NULL;
        if (stopArrPtr) {
            FreeAllStops(stopArrPtr->stops, stopArrPtr->nstops);
            stopArrPtr->nstops = 0;
        }
    } else {
        
        /* Deal with each stop list in turn. */
        if (Tcl_ListObjGetElements(interp, (*value), &nstops, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        
        /* Array of pointers to GradientStop. */
        stops = (GradientStop **) ckalloc(nstops*sizeof(GradientStop *));
        memset(stops, '\0', nstops*sizeof(GradientStop *));

        lastOffset = 0.0;
        
        for (i = 0; i < nstops; i++) {
            stopObj = objv[i];
            if (Tcl_ListObjLength(interp, stopObj, &stopLen) != TCL_OK) {
                goto error;
            }
            if ((stopLen == 2) || (stopLen == 3)) {
                Tcl_ListObjIndex(interp, stopObj, 0, &obj);
                if (Tcl_GetDoubleFromObj(interp, obj, &offset) != TCL_OK) {
                    goto error;
                }
                if ((offset < 0.0) || (offset > 1.0)) {
                    Tcl_SetObjResult(interp, Tcl_NewStringObj(
                            "stop offsets must be in the range 0.0 to 1.0", -1));
                    goto error;
                }
                if (offset < lastOffset) {
                    Tcl_SetObjResult(interp, Tcl_NewStringObj(
                            "stop offsets must be ordered", -1));
                    goto error;
                }
                Tcl_ListObjIndex(interp, stopObj, 1, &obj);
                color = Tk_AllocColorFromObj(interp, Tk_MainWindow(interp), obj);
                if (color == NULL) {
                    Tcl_AppendResult(interp, "color \"", 
                            Tcl_GetStringFromObj(obj, NULL), 
                            "\" doesn't exist", NULL);
                    goto error;
                }
                if (stopLen == 3) {
                    Tcl_ListObjIndex(interp, stopObj, 2, &obj);
                    if (Tcl_GetDoubleFromObj(interp, obj, &opacity) != TCL_OK) {
                        goto error;
                    }
                } else {
                    opacity = 1.0;
                }
                stops[i] = NewGradientStop(offset, color, opacity);
                lastOffset = offset;
            } else {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "stop list not {offset color ?opacity?}", -1));
                goto error;
            }
        }
        if (stopArrPtr != NULL) {
            stopArrPtr->nstops = nstops;
            stopArrPtr->stops = stops;
        }
    }
    return TCL_OK;
    
error:
    /* @@@ Not sure about this. */
    if (stopArrPtr) {
        FreeAllStops(stopArrPtr->stops, stopArrPtr->nstops);
        stopArrPtr->nstops = 0;
    }
    return TCL_ERROR;
}

#if 0
// It crashes
static void StopsFree(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)
{
    if (*((char **) internalPtr) != NULL) {
        GradientStopArray *stopArrPtr = (GradientStopArray *) internalPtr;
        FreeAllStops(stopArrPtr->stops, stopArrPtr->nstops);
        stopArrPtr->nstops = 0;
    }    
}
#endif

static Tk_ObjCustomOption stopsCO = 
{
    "stops",
    StopsSet,
    NULL,
    NULL,
    NULL,
    (ClientData) NULL
};

/*
 * The following table defines the legal values for the -method option.
 * The enum kPathGradientMethodPad... MUST be kept in sync!
 */

static char *methodST[] = {
    "pad", "repeat", "reflect", (char *) NULL
};

static Tk_OptionSpec linGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(LinearGradientStyle, fill.method), 
        0, (ClientData) methodST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(LinearGradientStyle, stopsObj),
        Tk_Offset(LinearGradientStyle, fill.stopArr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-lineartransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(LinearGradientStyle, transObj), 
        Tk_Offset(LinearGradientStyle, fill.transition),
		TK_OPTION_NULL_OK, (ClientData) &linTransitionCO, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static Tk_OptionSpec radGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(RadialGradientStyle, fill.method), 
        0, (ClientData) methodST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(RadialGradientStyle, stopsObj),
        Tk_Offset(RadialGradientStyle, fill.stopArr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-radialtransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(RadialGradientStyle, transObj), 
        Tk_Offset(RadialGradientStyle, fill),
		TK_OPTION_NULL_OK, (ClientData) &radTransitionCO, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

#if 0
static void 
FormatResult(Tcl_Interp *interp, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
}
#endif

#if 0
static int 
GetGradientStyleFromObj(Tcl_Interp *interp, Tcl_Obj *obj, LinearGradientStyle **stylePtrPtr)
{
	char *name;
	Tcl_HashEntry *hPtr;

	name = Tcl_GetString(obj);
	hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, name);
	if (hPtr == NULL) {
		Tcl_AppendResult(interp, 
                "lineargradient \"", name, "\" doesn't exist", NULL);
		return TCL_ERROR;
	}
	*stylePtrPtr = (LinearGradientStyle *) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}
#endif

#if 0
static int
GetLinearGradientFromNameObj(Tcl_Interp *interp, Tcl_Obj *obj, LinearGradientFill **gradientPtrPtr)
{
    LinearGradientStyle *gradientStylePtr;
    
    if (GetGradientStyleFromObj(interp, obj, &gradientStylePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    *gradientPtrPtr = &(gradientStylePtr->fill);
    return TCL_OK;
}
#endif

static int
GetLinearGradientFillFromName(char *name, LinearGradientFill **gradientPtrPtr)
{
	Tcl_HashEntry *hPtr;
    LinearGradientStyle *gradientStylePtr;

	hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, name);
	if (hPtr == NULL) {
        *gradientPtrPtr = NULL;
        return TCL_ERROR;
    } else {
        gradientStylePtr = (LinearGradientStyle *) Tcl_GetHashValue(hPtr);
        *gradientPtrPtr = &(gradientStylePtr->fill);
        return TCL_OK;
    }
}

int
HaveLinearGradientStyleWithName(CONST char *name)
{
    return (Tcl_FindHashEntry(gLinearGradientHashPtr, name) == NULL) ? TCL_ERROR : TCL_OK;
}

void
PathPaintLinearGradientFromName(TkPathContext ctx, PathRect *bbox, char *name, int fillRule)
{
    LinearGradientFill *fillPtr;
    if (GetLinearGradientFillFromName(name, &fillPtr) != TCL_OK) {
        return;
    }
    TkPathPaintLinearGradient(ctx, bbox, fillPtr, fillRule);
}

void
PathGradientInit(Tcl_Interp* interp) 
{
    gLinearGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    gRadialGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable(gLinearGradientHashPtr, TCL_STRING_KEYS);
    Tcl_InitHashTable(gRadialGradientHashPtr, TCL_STRING_KEYS);
    
    /*
     * The option table must only be made once and not for each instance.
     */
    gLinearGradientOptionTable = Tk_CreateOptionTable(interp, 
            linGradientStyleOptionSpecs);
    gRadialGradientOptionTable = Tk_CreateOptionTable(interp, 
            radGradientStyleOptionSpecs);
}

static void
FreeLinearGradientStyle(LinearGradientStyle *gradientStylePtr)
{
    FreeAllStops(gradientStylePtr->fill.stopArr.stops, gradientStylePtr->fill.stopArr.nstops);
	Tk_FreeConfigOptions((char *) gradientStylePtr, gradientStylePtr->optionTable, NULL);
	ckfree( (char *) gradientStylePtr );
}

static void
FreeRadialGradientStyle(RadialGradientStyle *gradientStylePtr)
{
    FreeAllStops(gradientStylePtr->fill.stopArr.stops, gradientStylePtr->fill.stopArr.nstops);
	Tk_FreeConfigOptions((char *) gradientStylePtr, gradientStylePtr->optionTable, NULL);
	ckfree( (char *) gradientStylePtr );
}

/*
 *----------------------------------------------------------------------
 *
 * LinearGradientCmd --
 *
 *		Implements the tkpath::lineargradient command.  
 *
 * Results:
 *		Standard Tcl result
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

int 
LinearGradientCmd( 
        ClientData clientData,
        Tcl_Interp* interp,
        int objc,
      	Tcl_Obj* CONST objv[] )
{
    return PathGenericCmdDispatcher(interp, objc, objv, 
            kLinearGradientNameBase, &gLinearGradientNameUid, 
            gLinearGradientHashPtr, gLinearGradientOptionTable,
            LinGradientCreateAndConfig, NULL, LinGradientFree);
}

static char *
LinGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[])
{
	LinearGradientStyle *gradientStylePtr;

	gradientStylePtr = (LinearGradientStyle *) ckalloc(sizeof(LinearGradientStyle));
	memset(gradientStylePtr, '\0', sizeof(LinearGradientStyle));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	gradientStylePtr->optionTable = gLinearGradientOptionTable; 
	gradientStylePtr->name = Tk_GetUid(name);
    
    /* Set default transition vector in case not set. */
    gradientStylePtr->fill.transition.x1 = 0.0;
    gradientStylePtr->fill.transition.y1 = 0.0;
    gradientStylePtr->fill.transition.x2 = 1.0;
    gradientStylePtr->fill.transition.y2 = 0.0;

	return (char *) gradientStylePtr;
}

static void
LinGradientFree(Tcl_Interp *interp, char *recordPtr) 
{
    FreeLinearGradientStyle((LinearGradientStyle *) recordPtr);
    ckfree(recordPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RadialGradientCmd --
 *
 *		Implements the tkpath::radialgradient command.  
 *
 * Results:
 *		Standard Tcl result
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

int 
RadialGradientCmd( 
        ClientData clientData,
        Tcl_Interp* interp,
        int objc,
      	Tcl_Obj* CONST objv[] )
{
    return PathGenericCmdDispatcher(interp, objc, objv, 
            kRadialGradientNameBase, &gRadialGradientNameUid, 
            gRadialGradientHashPtr, gRadialGradientOptionTable,
            RadGradientCreateAndConfig, NULL, RadGradientFree);
}

static char *
RadGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[])
{
	RadialGradientStyle *gradientStylePtr;

	gradientStylePtr = (RadialGradientStyle *) ckalloc(sizeof(RadialGradientStyle));
	memset(gradientStylePtr, '\0', sizeof(RadialGradientStyle));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	gradientStylePtr->optionTable = gRadialGradientOptionTable; 
	gradientStylePtr->name = Tk_GetUid(name);
    
    /* Set default transition vector in case not set. */
    gradientStylePtr->fill.centerX = 0.0;
    gradientStylePtr->fill.centerY = 0.0;
    gradientStylePtr->fill.rad = 1.0;
    gradientStylePtr->fill.focalX = 0.0;
    gradientStylePtr->fill.focalY = 0.0;

	return (char *) gradientStylePtr;
}

static void
RadGradientFree(Tcl_Interp *interp, char *recordPtr) 
{
    FreeRadialGradientStyle((RadialGradientStyle *) recordPtr);
    ckfree(recordPtr);
}
