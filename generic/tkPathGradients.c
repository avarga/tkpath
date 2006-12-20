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
 */
 
static int LinTransitionSet(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interp; may be used for errors. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
                             * We use a pointer to the pointer because
                             * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
                               internal value is to be stored. */
    char *oldInternalPtr,	/* Pointer to storage for the old value. */
    int flags)				/* Flags for the option, set Tk_SetOptions. */
{
    char *internalPtr;
    int objEmpty = 0;
    Tcl_Obj *valuePtr;
    double z[4] = {0.0, 0.0, 1.0, 0.0};		/* Defaults according to SVG. */
    PathRect *new = NULL;
    
    valuePtr = *value;
    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    objEmpty = ObjectIsEmpty(valuePtr);
    
    /*
     * Important: the new value for the transition is not yet 
     * stored into the style! transObj may be NULL!
     * The new value is stored in style *after* we return TCL_OK.
     */
    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        valuePtr = NULL;
    } else {
        int i, len;
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, valuePtr, &len, &objv) != TCL_OK) {
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
            /*
            if ((z[i] < 0.0) || (z[i] > 1.0)) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "-lineartransition elements must be in the range 0.0 to 1.0", -1));
                return TCL_ERROR;
            }
            */
        }
        new = (PathRect *) ckalloc(sizeof(PathRect));
        new->x1 = z[0];
        new->y1 = z[1];
        new->x2 = z[2];
        new->y2 = z[3];
    }
    if (internalPtr != NULL) {
        *((PathRect **) oldInternalPtr) = *((PathRect **) internalPtr);
        *((PathRect **) internalPtr) = new;
    }
    return TCL_OK;
}

static void
LinTransitionRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(PathRect **)internalPtr = *(PathRect **)oldInternalPtr;
}

static void
LinTransitionFree(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

static Tk_ObjCustomOption linTransitionCO = 
{
    "lineartransition",
    LinTransitionSet,
    NULL,
    LinTransitionRestore,
    LinTransitionFree,
    (ClientData) NULL
};

static int RadTransitionSet(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interp; may be used for errors. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
                             * We use a pointer to the pointer because
                             * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
                               internal value is to be stored. */
    char *oldInternalPtr,	/* Pointer to storage for the old value. */
    int flags)				/* Flags for the option, set Tk_SetOptions. */
{
    char *internalPtr;
    int objEmpty = 0;
    Tcl_Obj *valuePtr;
    double z[5] = {0.5, 0.5, 0.5, 0.5, 0.5};
    RadialTransition *new = NULL;

    valuePtr = *value;
    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    objEmpty = ObjectIsEmpty(valuePtr);
    
    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        valuePtr = NULL;
    } else {
        int i, len;
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, valuePtr, &len, &objv) != TCL_OK) {
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
        new = (RadialTransition *) ckalloc(sizeof(RadialTransition));
        new->centerX = z[0];
        new->centerY = z[1];
        new->radius = z[2];
        new->focalX = z[3];
        new->focalY = z[4];
    }
    if (internalPtr != NULL) {
        *((RadialTransition **) oldInternalPtr) = *((RadialTransition **) internalPtr);
        *((RadialTransition **) internalPtr) = new;
    }
    return TCL_OK;
}

static void
RadTransitionRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(RadialTransition **)internalPtr = *(RadialTransition **)oldInternalPtr;
}

static void
RadTransitionFree(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

static Tk_ObjCustomOption radTransitionCO = 
{
    "radialtransition",
    RadTransitionSet,
    NULL,
    RadTransitionRestore,
    RadTransitionFree,
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

static GradientStopArray *NewGradientStopArray(int nstops)
{
    GradientStopArray *stopArrPtr;
    GradientStop **stops;

	stopArrPtr = (GradientStopArray *) ckalloc(sizeof(GradientStopArray));
	memset(stopArrPtr, '\0', sizeof(GradientStopArray));

    /* Array of *pointers* to GradientStop. */
    stops = (GradientStop **) ckalloc(nstops*sizeof(GradientStop *));
    memset(stops, '\0', nstops*sizeof(GradientStop *));
    stopArrPtr->nstops = nstops;
    stopArrPtr->stops = stops;
    return stopArrPtr;
}

static void
FreeAllStops(GradientStop **stops, int nstops)
{
    int i;
    for (i = 0; i < nstops; i++) {
        if (stops[i] != NULL) {
            /* @@@ Free color? */
            ckfree((char *) (stops[i]));
        }
    }
    ckfree((char *) stops);
}

static void
FreeStopArray(GradientStopArray *stopArrPtr)
{
    if (stopArrPtr != NULL) {
        FreeAllStops(stopArrPtr->stops, stopArrPtr->nstops);
        ckfree((char *) stopArrPtr);
    }
}

/*
 * The stops are a list of stop lists where each stop list is:
 *		{offset color ?opacity?}
 */
static int StopsSet(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interp; may be used for errors. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
                             * We use a pointer to the pointer because
                             * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
                               internal value is to be stored. */
    char *oldInternalPtr,	/* Pointer to storage for the old value. */
    int flags)				/* Flags for the option, set Tk_SetOptions. */
{
    char *internalPtr;
    int i, nstops, stopLen;
    int objEmpty = 0;
    Tcl_Obj *valuePtr;
    double offset, lastOffset, opacity;
    Tcl_Obj **objv;
    Tcl_Obj *stopObj;
    Tcl_Obj *obj;
    XColor *color;
    GradientStopArray *new = NULL;
    
    valuePtr = *value;
    internalPtr = ComputeSlotAddress(recordPtr, internalOffset);
    objEmpty = ObjectIsEmpty(valuePtr);

    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        valuePtr = NULL;
    } else {
        
        /* Deal with each stop list in turn. */
        if (Tcl_ListObjGetElements(interp, valuePtr, &nstops, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        new = NewGradientStopArray(nstops);
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
                
                /* Make new stop. */
                new->stops[i] = NewGradientStop(offset, color, opacity);
                lastOffset = offset;
            } else {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "stop list not {offset color ?opacity?}", -1));
                goto error;
            }
        }
    }
    if (internalPtr != NULL) {
        *((GradientStopArray **) oldInternalPtr) = *((GradientStopArray **) internalPtr);
        *((GradientStopArray **) internalPtr) = new;
    }
    return TCL_OK;
    
error:
    if (new != NULL) {
        FreeStopArray(new);
    }
    return TCL_ERROR;
}

static void
StopsRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(GradientStopArray **)internalPtr = *(GradientStopArray **)oldInternalPtr;
}

static void StopsFree(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)
{
    if (*((char **) internalPtr) != NULL) {
        FreeStopArray(*(GradientStopArray **)internalPtr);
    }    
}

static Tk_ObjCustomOption stopsCO = 
{
    "stops",
    StopsSet,
    NULL,
    StopsRestore,
    StopsFree,
    (ClientData) NULL
};

/*
 * The following table defines the legal values for the -method option.
 * The enum kPathGradientMethodPad... MUST be kept in sync!
 */

static char *methodST[] = {
    "pad", "repeat", "reflect", (char *) NULL
};
static char *unitsST[] = {
    "bbox", "userspace", (char *) NULL
};

static Tk_OptionSpec linGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(LinearGradientStyle, fill.method), 
        0, (ClientData) methodST, 0},
    {TK_OPTION_STRING_TABLE, "-units", (char *) NULL, (char *) NULL,
        "bbox", -1, Tk_Offset(LinearGradientStyle, fill.units), 
        0, (ClientData) unitsST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(LinearGradientStyle, stopsObj),
        Tk_Offset(LinearGradientStyle, fill.stopArrPtr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-lineartransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(LinearGradientStyle, transObj), 
        Tk_Offset(LinearGradientStyle, fill.transitionPtr),
		TK_OPTION_NULL_OK, (ClientData) &linTransitionCO, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static Tk_OptionSpec radGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(RadialGradientStyle, fill.method), 
        0, (ClientData) methodST, 0},
    {TK_OPTION_STRING_TABLE, "-units", (char *) NULL, (char *) NULL,
        "bbox", -1, Tk_Offset(RadialGradientStyle, fill.units), 
        0, (ClientData) unitsST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(RadialGradientStyle, stopsObj),
        Tk_Offset(RadialGradientStyle, fill.stopArrPtr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-radialtransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(RadialGradientStyle, transObj), 
        Tk_Offset(RadialGradientStyle, fill.radialPtr),
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

static int
GetRadialGradientFillFromName(char *name, RadialGradientFill **gradientPtrPtr)
{
	Tcl_HashEntry *hPtr;
    RadialGradientStyle *gradientStylePtr;

	hPtr = Tcl_FindHashEntry(gRadialGradientHashPtr, name);
	if (hPtr == NULL) {
        *gradientPtrPtr = NULL;
        return TCL_ERROR;
    } else {
        gradientStylePtr = (RadialGradientStyle *) Tcl_GetHashValue(hPtr);
        *gradientPtrPtr = &(gradientStylePtr->fill);
        return TCL_OK;
    }
}

static int
HaveLinearGradientStyleWithName(CONST char *name)
{
    return (Tcl_FindHashEntry(gLinearGradientHashPtr, name) == NULL) ? TCL_ERROR : TCL_OK;
}

static int
HaveRadialGradientStyleWithName(CONST char *name)
{
    return (Tcl_FindHashEntry(gRadialGradientHashPtr, name) == NULL) ? TCL_ERROR : TCL_OK;
}

int
HaveGradientStyleWithName(CONST char *name)
{
    if ((HaveLinearGradientStyleWithName(name) == TCL_OK) 
            || (HaveRadialGradientStyleWithName(name) == TCL_OK)) {
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}

void
PathPaintGradientFromName(TkPathContext ctx, PathRect *bbox, char *name, int fillRule)
{
    LinearGradientFill *linearFillPtr;
    RadialGradientFill *radialFillPtr;

    if (GetLinearGradientFillFromName(name, &linearFillPtr) == TCL_OK) {
        TkPathPaintLinearGradient(ctx, bbox, linearFillPtr, fillRule);
    } else if (GetRadialGradientFillFromName(name, &radialFillPtr) == TCL_OK) {
        TkPathPaintRadialGradient(ctx, bbox, radialFillPtr, fillRule);
    }
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
    FreeStopArray(gradientStylePtr->fill.stopArrPtr);
	Tk_FreeConfigOptions((char *) gradientStylePtr, gradientStylePtr->optionTable, NULL);
	ckfree( (char *) gradientStylePtr );
}

static void
FreeRadialGradientStyle(RadialGradientStyle *gradientStylePtr)
{
    FreeStopArray(gradientStylePtr->fill.stopArrPtr);
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
    PathRect *transitionPtr = NULL;

	gradientStylePtr = (LinearGradientStyle *) ckalloc(sizeof(LinearGradientStyle));
	memset(gradientStylePtr, '\0', sizeof(LinearGradientStyle));
    transitionPtr = (PathRect *) ckalloc(sizeof(PathRect));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	gradientStylePtr->optionTable = gLinearGradientOptionTable; 
	gradientStylePtr->name = Tk_GetUid(name);
    gradientStylePtr->fill.transitionPtr = transitionPtr;
    
    /* Set default transition vector in case not set. */
    transitionPtr->x1 = 0.0;
    transitionPtr->y1 = 0.0;
    transitionPtr->x2 = 1.0;
    transitionPtr->y2 = 0.0;
    
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
    RadialTransition *tPtr = NULL;

	gradientStylePtr = (RadialGradientStyle *) ckalloc(sizeof(RadialGradientStyle));
	memset(gradientStylePtr, '\0', sizeof(RadialGradientStyle));
    tPtr = (RadialTransition *) ckalloc(sizeof(RadialTransition));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	gradientStylePtr->optionTable = gRadialGradientOptionTable; 
	gradientStylePtr->name = Tk_GetUid(name);
    gradientStylePtr->fill.radialPtr = tPtr;
    
    /* Set default transition vector in case not set. */
    tPtr->centerX = 0.5;
    tPtr->centerY = 0.5;
    tPtr->radius = 0.5;
    tPtr->focalX = 0.5;
    tPtr->focalY = 0.5;

	return (char *) gradientStylePtr;
}

static void
RadGradientFree(Tcl_Interp *interp, char *recordPtr) 
{
    FreeRadialGradientStyle((RadialGradientStyle *) recordPtr);
    ckfree(recordPtr);
}
