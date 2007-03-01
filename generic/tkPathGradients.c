/*
 * tkPathGradients.c --
 *
 *	    This file implements gradient objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2007  Mats Bengtsson
 *
 * NB:   It would be best to have this in the canvas widget as a special
 *       object, but I see no way of doing this without touching
 *       the canvas code.
 *
 * NB:   When a gradient object is modified or destroyed the corresponding
 *       items are not notified. They will only notice any change when
 *       they need to redisplay.
 *
 * NB:   Right now we have duplicated gradient commands and a lot of related stuff.
 *
 * TODO: o Add tkwin option here and there so we can free stop colors!
 *       o Clean out all linear/radial specific code related to tkpath::lineargradient
 *		   and tkpath::radialgradient commands.
 *
 * $Id$
 */

#include "tkIntPath.h"

/*
 * Hash table to keep track of linear gradient fills.
 */    

Tcl_HashTable 	*gGradientHashPtr = NULL;
Tcl_HashTable 	*gLinearGradientHashPtr = NULL;
Tcl_HashTable 	*gRadialGradientHashPtr = NULL;

static Tk_OptionTable 	gLinearGradientOptionTableOLD;
static Tk_OptionTable 	gRadialGradientOptionTableOLD;
static Tk_OptionTable 	gLinearGradientOptionTable;
static Tk_OptionTable 	gRadialGradientOptionTable;
static int 				gGradientNameUid = 0;
static int 				gLinearGradientNameUid = 0;
static int 				gRadialGradientNameUid = 0;
static char *			kGradientNameBase = "gradient";
static char *			kLinearGradientNameBase = "lineargradient";
static char *			kRadialGradientNameBase = "radialgradient";

enum {
    kPathGradientTypeLinear =	0L,
    kPathGradientTypeRadial
};

typedef struct GradientStyle {
    int type;						/* Any of kPathGradientTypeLinear or kPathGradientTypeRadial */
	Tk_OptionTable optionTable;
	Tk_Uid name;
    Tcl_Obj *transObj;
    Tcl_Obj *stopsObj;
    union {
        LinearGradientFill linearFill;
        RadialGradientFill radialFill;
    };
} GradientStyle;

// OUTDATED
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

static int 			GradientObjCmd(ClientData clientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* CONST objv[]);
static void			GradientStyleFree(Tcl_Interp *interp, GradientStyle *gradientStylePtr);

// OUTDATED
static int			LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                            int objc, Tcl_Obj* CONST objv[]);
static char *		LinGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]);
static void			LinGradientFree(Tcl_Interp *interp, char *recordPtr);
static int			RadialGradientCmd(ClientData clientData, Tcl_Interp* interp,
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

static Tk_OptionSpec linGradientStyleOptionSpecsOLD[] = {
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

static Tk_OptionSpec linGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(GradientStyle, linearFill.method), 
        0, (ClientData) methodST, 0},
    {TK_OPTION_STRING_TABLE, "-units", (char *) NULL, (char *) NULL,
        "bbox", -1, Tk_Offset(GradientStyle, linearFill.units), 
        0, (ClientData) unitsST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(GradientStyle, stopsObj),
        Tk_Offset(GradientStyle, linearFill.stopArrPtr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-lineartransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(GradientStyle, transObj), 
        Tk_Offset(GradientStyle, linearFill.transitionPtr),
		TK_OPTION_NULL_OK, (ClientData) &linTransitionCO, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static Tk_OptionSpec radGradientStyleOptionSpecsOLD[] = {
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

static Tk_OptionSpec radGradientStyleOptionSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-method", (char *) NULL, (char *) NULL,
        "pad", -1, Tk_Offset(GradientStyle, radialFill.method), 
        0, (ClientData) methodST, 0},
    {TK_OPTION_STRING_TABLE, "-units", (char *) NULL, (char *) NULL,
        "bbox", -1, Tk_Offset(GradientStyle, radialFill.units), 
        0, (ClientData) unitsST, 0},
	{TK_OPTION_CUSTOM, "-stops", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(GradientStyle, stopsObj),
        Tk_Offset(GradientStyle, radialFill.stopArrPtr),
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-radialtransition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(GradientStyle, transObj), 
        Tk_Offset(GradientStyle, radialFill.radialPtr),
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
GetLinearGradientFillFromNameOLD(char *name, LinearGradientFill **gradientPtrPtr)
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
GetRadialGradientFillFromNameOLD(char *name, RadialGradientFill **gradientPtrPtr)
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

int
HaveGradientStyleWithName(CONST char *name)
{
    return (Tcl_FindHashEntry(gGradientHashPtr, name) == NULL) ? TCL_ERROR : TCL_OK;
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
HaveGradientStyleWithNameOLD(CONST char *name)
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
	Tcl_HashEntry *hPtr;
    GradientStyle *gradientStylePtr;

	hPtr = Tcl_FindHashEntry(gGradientHashPtr, name);
    if (hPtr) {
        gradientStylePtr = (GradientStyle *) Tcl_GetHashValue(hPtr);
        if (gradientStylePtr->type == kPathGradientTypeLinear) {
            TkPathPaintLinearGradient(ctx, bbox, &(gradientStylePtr->linearFill), fillRule);
        } else {
            TkPathPaintRadialGradient(ctx, bbox, &(gradientStylePtr->radialFill), fillRule);
        }
    }
}

void
PathPaintGradientFromNameOLD(TkPathContext ctx, PathRect *bbox, char *name, int fillRule)
{
    LinearGradientFill *linearFillPtr;
    RadialGradientFill *radialFillPtr;

    if (GetLinearGradientFillFromNameOLD(name, &linearFillPtr) == TCL_OK) {
        TkPathPaintLinearGradient(ctx, bbox, linearFillPtr, fillRule);
    } else if (GetRadialGradientFillFromNameOLD(name, &radialFillPtr) == TCL_OK) {
        TkPathPaintRadialGradient(ctx, bbox, radialFillPtr, fillRule);
    }
}

void
PathGradientInit(Tcl_Interp* interp) 
{
    gGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    gLinearGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    gRadialGradientHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable(gGradientHashPtr, TCL_STRING_KEYS);
    Tcl_InitHashTable(gLinearGradientHashPtr, TCL_STRING_KEYS);
    Tcl_InitHashTable(gRadialGradientHashPtr, TCL_STRING_KEYS);
    
    /*
     * The option table must only be made once and not for each instance.
     */
    gLinearGradientOptionTableOLD = Tk_CreateOptionTable(interp, 
            linGradientStyleOptionSpecsOLD);
    gRadialGradientOptionTableOLD = Tk_CreateOptionTable(interp, 
            radGradientStyleOptionSpecsOLD);
            
    gLinearGradientOptionTable = Tk_CreateOptionTable(interp, 
            linGradientStyleOptionSpecs);
    gRadialGradientOptionTable = Tk_CreateOptionTable(interp, 
            radGradientStyleOptionSpecs);

    Tcl_CreateObjCommand(interp, "::tkpath::gradient",
            GradientObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
#if 0	// OLD
    Tcl_CreateObjCommand(interp, "::tkpath::lineargradient",
            LinearGradientCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "::tkpath::radialgradient",
            RadialGradientCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
#endif
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

static CONST char *gradientCmds[] = {
    "cget", "configure", "create", "delete", "names", "type",
    (char *) NULL
};

enum {
	kPathGradientCmdCget						= 0L,
    kPathGradientCmdConfigure,
    kPathGradientCmdCreate,
    kPathGradientCmdDelete,
    kPathGradientCmdNames,
    kPathGradientCmdType
};

/*
 *----------------------------------------------------------------------
 *
 * GradientObjCmd --
 *
 *		Implements the tkpath::gradient command.  
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
GradientObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[])
{
    char   			*name;
    int 			result = TCL_OK;
    int 			index;
    int				mask;
    int				type;
    Tcl_HashEntry 	*hPtr;
    GradientStyle 	*gradientStylePtr;
    Tcl_Obj 		*resultObjPtr = NULL;
    Tk_Window 		tkwin = Tk_MainWindow(interp); /* Should have been the canvas. */

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg...?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], gradientCmds, "command", 0,
            &index) != TCL_OK) {
        return TCL_ERROR;
    }
    switch (index) {
    
        case kPathGradientCmdCget: {            
    		if (objc != 4) {
				Tcl_WrongNumArgs(interp, 3, objv, "option");
    			return TCL_ERROR;
    		}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(gGradientHashPtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, "gradient \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            gradientStylePtr = Tcl_GetHashValue(hPtr);
            type = gradientStylePtr->type;
			resultObjPtr = Tk_GetOptionValue(interp, (char *)gradientStylePtr, 
                    gradientStylePtr->optionTable, objv[3], tkwin);
			if (resultObjPtr == NULL) {
				result = TCL_ERROR;
			} else {
				Tcl_SetObjResult(interp, resultObjPtr);
			}
            break;
        }

        case kPathGradientCmdConfigure: {
			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(gGradientHashPtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, "gradient \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            gradientStylePtr = Tcl_GetHashValue(hPtr);
            type = gradientStylePtr->type;
			if (objc <= 4) {
				resultObjPtr = Tk_GetOptionInfo(interp, (char *)gradientStylePtr, 
                        gradientStylePtr->optionTable,
                        (objc == 3) ? (Tcl_Obj *) NULL : objv[3], tkwin);
				if (resultObjPtr == NULL) {
					return TCL_ERROR;
                }
				Tcl_SetObjResult(interp, resultObjPtr);
			} else {
				if (Tk_SetOptions(interp, (char *)gradientStylePtr, gradientStylePtr->optionTable, 
                        objc - 3, objv + 3, tkwin, NULL, &mask) != TCL_OK) {
					return TCL_ERROR;
                }
			}
            break;
        }

        case kPathGradientCmdCreate: {
			char str[255];
            char *typeStr;
			int isNew;

			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "type ?option value...?");
				return TCL_ERROR;
			}
            typeStr = Tcl_GetStringFromObj(objv[2], NULL);
            if (strcmp(typeStr, "linear") == 0) {
                type = kPathGradientTypeLinear;
            } else if (strcmp(typeStr, "radial") == 0) {
                type = kPathGradientTypeRadial;
            } else {
                Tcl_AppendResult(interp, "unrecognized type \"", typeStr, 
                        "\", must be \"linear\" or \"radial\"", NULL);
                return TCL_ERROR;
            }
            sprintf(str, "%s%d", kGradientNameBase, gGradientNameUid++);
            gradientStylePtr = (GradientStyle *) ckalloc(sizeof(GradientStyle));
            memset(gradientStylePtr, '\0', sizeof(GradientStyle));

            /*
             * Create the option table for this class.  If it has already
             * been created, the cached pointer will be returned.
             */
            if (type == kPathGradientTypeLinear) {
                gradientStylePtr->optionTable = gLinearGradientOptionTable; 
            } else {
                gradientStylePtr->optionTable = gRadialGradientOptionTable; 
            }
            gradientStylePtr->type = type;
            gradientStylePtr->name = Tk_GetUid(str);

            /* 
             * Set default transition vector in case not set. 
             */
            if (type == kPathGradientTypeLinear) {
                PathRect *transitionPtr;
                
                transitionPtr = (PathRect *) ckalloc(sizeof(PathRect));
                gradientStylePtr->linearFill.transitionPtr = transitionPtr;
                transitionPtr->x1 = 0.0;
                transitionPtr->y1 = 0.0;
                transitionPtr->x2 = 1.0;
                transitionPtr->y2 = 0.0;
            } else {
                RadialTransition *tPtr;
            
                tPtr = (RadialTransition *) ckalloc(sizeof(RadialTransition));
                gradientStylePtr->radialFill.radialPtr = tPtr;
                tPtr->centerX = 0.5;
                tPtr->centerY = 0.5;
                tPtr->radius = 0.5;
                tPtr->focalX = 0.5;
                tPtr->focalY = 0.5;
            }
            if (Tk_InitOptions(interp, (char *)gradientStylePtr, 
                    gradientStylePtr->optionTable, tkwin) != TCL_OK) {
                ckfree((char *)gradientStylePtr);
                return TCL_ERROR;
            }
            if (Tk_SetOptions(interp, (char *)gradientStylePtr, gradientStylePtr->optionTable, 	
                    objc - 3, objv + 3, tkwin, NULL, &mask) != TCL_OK) {
                Tk_FreeConfigOptions((char *)gradientStylePtr, gradientStylePtr->optionTable, NULL);
                ckfree((char *)gradientStylePtr);
                return TCL_ERROR;
            }
			hPtr = Tcl_CreateHashEntry(gGradientHashPtr, str, &isNew);
			Tcl_SetHashValue(hPtr, gradientStylePtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(str, -1));
            break;
        }

        case kPathGradientCmdDelete: {
			if (objc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name");
				return TCL_ERROR;
			}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(gGradientHashPtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, "gradient \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            Tcl_DeleteHashEntry(hPtr);
            gradientStylePtr = Tcl_GetHashValue(hPtr);
            GradientStyleFree(interp, gradientStylePtr);         
			break;
        }

        case kPathGradientCmdNames: {
			Tcl_Obj *listObj;
			Tcl_HashSearch search;

			if (objc != 2) {
				Tcl_WrongNumArgs(interp, 2, objv, NULL);
				return TCL_ERROR;
			}
			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(gGradientHashPtr, &search);
			while (hPtr != NULL) {
                name = Tcl_GetHashKey(gGradientHashPtr, hPtr);
				Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(name, -1));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
            break;
        }

        case kPathGradientCmdType: {
			if (objc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name");
				return TCL_ERROR;
			}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(gGradientHashPtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, "gradient \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            gradientStylePtr = Tcl_GetHashValue(hPtr);
            Tcl_SetObjResult(interp, Tcl_NewStringObj( 
                    (gradientStylePtr->type == kPathGradientTypeLinear) ? "linear" : "radial", -1));         
            break;
        }
    }
    return result;
}

static void
GradientStyleFree(Tcl_Interp *interp, GradientStyle *gradientStylePtr)
{
    if (gradientStylePtr->type == kPathGradientTypeLinear) {
        FreeStopArray(gradientStylePtr->linearFill.stopArrPtr);
    } else {
        FreeStopArray(gradientStylePtr->radialFill.stopArrPtr);
    }
	Tk_FreeConfigOptions((char *) gradientStylePtr, gradientStylePtr->optionTable, NULL);
    ckfree((char *) gradientStylePtr);
}

// OUTDATED

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
            gLinearGradientHashPtr, gLinearGradientOptionTableOLD,
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
	gradientStylePtr->optionTable = gLinearGradientOptionTableOLD; 
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
            gRadialGradientHashPtr, gRadialGradientOptionTableOLD,
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
	gradientStylePtr->optionTable = gRadialGradientOptionTableOLD; 
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
}
