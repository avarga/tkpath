/*
 * tkPathGradients.c --
 *
 *	    This file implements gradient objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
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

#include <tcl.h>
#include <tk.h>
#include <tkInt.h>
#include "tkPath.h"
#include "tkIntPath.h"

extern Tcl_HashTable 	*gLinearGradientHashPtr;
static int gStyleNameUid = 0;
static char *gStyleNameBase = "lineargradient";

typedef struct LinearGradientStyle {
	Tk_OptionTable optionTable;
	Tk_Uid name;
    Tcl_Obj *rectObj;
    Tcl_Obj *stopsObj;
    LinearGradientFill fill;
} LinearGradientStyle;

/*
 * For dispatching commands.
 */

static CONST char *gradientCmds[] = {
    "cget", "configure", "create", "delete", "names",
    (char *) NULL
};

enum {
	kPathGradientCmdCget						= 0L,
    kPathGradientCmdConfigure,
    kPathGradientCmdCreate,
    kPathGradientCmdDelete,
    kPathGradientCmdNames
};


static int	FillInTransitionFromRectObj(Tcl_Interp *interp, 
                    LinearGradientStyle *stylePtr, Tcl_Obj *rectObj);

int 	LinearGradientCmd(ClientData clientData, Tcl_Interp* interp,
                int objc, Tcl_Obj* CONST objv[]);
int		HaveLinearGradientStyleWithName(CONST char *name);


/* Cleanup all this when it works!!!!!!!!!!!!!!!!!!! */

static int ObjectIsEmpty(Tcl_Obj *obj)
{
    int length;

    if (obj == NULL)
        return 1;
    if (obj->bytes != NULL)
        return (obj->length == 0);
    Tcl_GetStringFromObj(obj, &length);
    return (length == 0);
}

/*
 * Procedures for processing the transition option of the gradient fill.
 * Boundaries are from 0.0 to 1.0.
 */
 
static int TransitionSet(
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
    LinearGradientStyle *stylePtr = (LinearGradientStyle *) recordPtr;
    
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }
    objEmpty = ObjectIsEmpty((*value));
    
    /*
     * Important: the new value for the transition is not yet 
     * stored into the style! rectObj may be NULL!
     * The new value is stored in style *after* we return TCL_OK.
     */
    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        (*value) = NULL;
        return TCL_OK;
    } else {
        int i, len;
        double z[4];
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, (*value), &len, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        if (len != 4) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(
                    "-transition must have four elements", -1));
            return TCL_ERROR;
        }
        for (i = 0; i < 4; i++) {
            if (Tcl_GetDoubleFromObj(interp, objv[i], z+i) != TCL_OK) {
                return TCL_ERROR;
            }
            if ((z[i] < 0.0) || (z[i] > 1.0)) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "-transition elements must be in the range 0.0 to 1.0", -1));
                return TCL_ERROR;
            }
        }
        return FillInTransitionFromRectObj(interp, stylePtr, (*value));
    }
}

static Tk_ObjCustomOption transitionCO = 
{
    "transition",
    TransitionSet,
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

static void
FreeLinearGradientStyle(LinearGradientStyle *stylePtr)
{
    FreeAllStops(stylePtr->fill.stops, stylePtr->fill.nstops);
	Tk_FreeConfigOptions((char *) stylePtr, stylePtr->optionTable, NULL);
	ckfree( (char *) stylePtr );
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
    LinearGradientStyle *stylePtr = (LinearGradientStyle *) recordPtr;
    LinearGradientFill *fillPtr = &(stylePtr->fill);
    
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }
    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
        (*value) = NULL;
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
        fillPtr->stops = stops;
        fillPtr->nstops = nstops;
    }
    return TCL_OK;
    
error:
    FreeAllStops(stops, nstops);
    fillPtr->nstops = 0;
    return TCL_ERROR;
}

static Tk_ObjCustomOption stopsCO = 
{
    "stops",
    StopsSet,
    NULL,
    NULL,
    NULL,			/* We would perhaps need this to be able to
                     * free up the stops record. */
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
		(char *) NULL, Tk_Offset(LinearGradientStyle, stopsObj), -1,
		TK_OPTION_NULL_OK, (ClientData) &stopsCO, 0},
	{TK_OPTION_CUSTOM, "-transition", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(LinearGradientStyle, rectObj), -1,
		TK_OPTION_NULL_OK, (ClientData) &transitionCO, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};


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

static int
FillInTransitionFromRectObj(Tcl_Interp *interp, LinearGradientStyle *stylePtr, Tcl_Obj *rectObj)
{
    LinearGradientFill *fillPtr = &(stylePtr->fill);
    
    /* The default is left to right fill. */
    if (rectObj == NULL) {
        fillPtr->transition.x1 = 0.0;
        fillPtr->transition.y1 = 0.0;
        fillPtr->transition.x2 = 1.0;
        fillPtr->transition.y2 = 0.0;
    } else {
        int i, len;
        double z[4];
        Tcl_Obj **objv;
        
        if (Tcl_ListObjGetElements(interp, rectObj, &len, &objv) != TCL_OK) {
            return TCL_ERROR;
        }
        for (i = 0; i < 4; i++) {
            if (Tcl_GetDoubleFromObj(interp, objv[i], z+i) != TCL_OK) {
                return TCL_ERROR;
            }
        }
        fillPtr->transition.x1 = z[0];
        fillPtr->transition.y1 = z[1];
        fillPtr->transition.x2 = z[2];
        fillPtr->transition.y2 = z[3];
    }
    return TCL_OK;
}

static LinearGradientStyle *
LinGradientCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[])
{
	LinearGradientStyle *stylePtr;

	stylePtr = (LinearGradientStyle *) ckalloc(sizeof(LinearGradientStyle));
	memset(stylePtr, '\0', sizeof(LinearGradientStyle));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	stylePtr->optionTable = Tk_CreateOptionTable(interp, linGradientStyleOptionSpecs); 
	stylePtr->name = Tk_GetUid(name);
    
    /* Set default transition vector in case not set. */
    FillInTransitionFromRectObj(interp, stylePtr, NULL);

	if (Tk_InitOptions(interp, (char *) stylePtr, stylePtr->optionTable, 
            NULL) != TCL_OK) {
		ckfree((char *) stylePtr);
		return NULL;
	}

	if (Tk_SetOptions(interp, (char *) stylePtr, stylePtr->optionTable, 
            objc, objv, NULL, NULL, NULL) != TCL_OK) {
		Tk_FreeConfigOptions((char *) stylePtr, stylePtr->optionTable, NULL);
		ckfree((char *) stylePtr);
		return NULL;
	}
	return stylePtr;
}

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

static int
GetLinearGradientFromNameObj(Tcl_Interp *interp, Tcl_Obj *obj, LinearGradientFill **gradientPtrPtr)
{
    LinearGradientStyle *stylePtr;
    
    if (GetGradientStyleFromObj(interp, obj, &stylePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    *gradientPtrPtr = &(stylePtr->fill);
    return TCL_OK;
}

int
GetLinearGradientFromName(char *name, LinearGradientFill **gradientPtrPtr)
{
	Tcl_HashEntry *hPtr;
    LinearGradientStyle *stylePtr;

	hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, name);
	if (hPtr == NULL) {
        *gradientPtrPtr = NULL;
        return TCL_ERROR;
    } else {
        stylePtr = (LinearGradientStyle *) Tcl_GetHashValue(hPtr);
        *gradientPtrPtr = &(stylePtr->fill);
        return TCL_OK;
    }
}

int
HaveLinearGradientStyleWithName(CONST char *name)
{
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, name);
	if (hPtr == NULL) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

static void 
FreeStyle(LinearGradientStyle *stylePtr)
{
	Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, stylePtr->name);
    if (hPtr != NULL) {
		Tcl_DeleteHashEntry(hPtr);
	}
    FreeLinearGradientStyle(stylePtr);
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
    int result = TCL_OK;
    int index;
    LinearGradientStyle *stylePtr = NULL;

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
            Tcl_Obj *resultObjPtr = NULL;
            
    		if (objc != 4) {
				Tcl_WrongNumArgs( interp, 3, objv, "option" );
    			return TCL_ERROR;
    		}
			if (GetGradientStyleFromObj(interp, objv[2], &stylePtr) != TCL_OK) {
				return TCL_ERROR;
            }
			resultObjPtr = Tk_GetOptionValue(interp, (char *) stylePtr,
                    stylePtr->optionTable, objv[3], NULL);
			if (resultObjPtr == NULL) {
				result = TCL_ERROR;
			} else {
				Tcl_SetObjResult( interp, resultObjPtr );
			}
            break;
        }
        
        case kPathGradientCmdConfigure: {
			Tcl_Obj *resultObjPtr = NULL;

			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
			if (GetGradientStyleFromObj(interp, objv[2], &stylePtr) != TCL_OK) {
				return TCL_ERROR;
            }
			if (objc <= 4) {
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) stylePtr,
                        stylePtr->optionTable,
                        (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
                        NULL);
				if (resultObjPtr == NULL) {
					return TCL_ERROR;
                }
				Tcl_SetObjResult(interp, resultObjPtr);
			} else {
				if (Tk_SetOptions(interp, (char *) stylePtr,
                        stylePtr->optionTable, objc - 3, objv + 3, NULL,
                        NULL, NULL) != TCL_OK) {
					return TCL_ERROR;
                }
                
			}
            break;
        
        case kPathGradientCmdCreate: {
        
            /*
             * Create with auto generated unique name.
             */
			char name[255];
			Tcl_HashEntry *hPtr;
			int isNew;

			if (objc < 2) {
				Tcl_WrongNumArgs(interp, 2, objv, "?option value...?");
				return TCL_ERROR;
			}
            sprintf(name, "%s%d", gStyleNameBase, gStyleNameUid);
            gStyleNameUid++;
			stylePtr = LinGradientCreateAndConfig(interp, name, objc - 2, objv + 2);
			if (stylePtr == NULL) {
				return TCL_ERROR;
            }
			hPtr = Tcl_CreateHashEntry(gLinearGradientHashPtr, name, &isNew);
			Tcl_SetHashValue(hPtr, stylePtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));
            break;
        }
        }
        
        case -99: {
            /*
             * Create using named style. I don't like this.
             */
			char *name;
			int len;
			Tcl_HashEntry *hPtr;
			int isNew;

			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[2], &len);
			if (!len) {
				FormatResult(interp, "invalid style name \"\"");
				return TCL_ERROR;
			}
			hPtr = Tcl_FindHashEntry(gLinearGradientHashPtr, name);
			if (hPtr != NULL) {
				FormatResult(interp, "style \"%s\" already exists", name);
				return TCL_ERROR;
			}
			stylePtr = LinGradientCreateAndConfig(interp, name, objc - 3, objv + 3);
			if (stylePtr == NULL) {
				return TCL_ERROR;
            }
			hPtr = Tcl_CreateHashEntry(gLinearGradientHashPtr, name, &isNew);
			Tcl_SetHashValue(hPtr, stylePtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));
            break;
        }
        
        case kPathGradientCmdDelete: {
			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name");
				return TCL_ERROR;
			}
			if (GetGradientStyleFromObj(interp, objv[2], &stylePtr) != TCL_OK) {
				return TCL_ERROR;
            }
            FreeStyle(stylePtr);
			break;
        }
        
        case kPathGradientCmdNames: {
			Tcl_Obj *listObj;
			Tcl_HashSearch search;
			Tcl_HashEntry *hPtr;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(gLinearGradientHashPtr, &search);
			while (hPtr != NULL) {
                stylePtr = (LinearGradientStyle *) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj, 
                        Tcl_NewStringObj(stylePtr->name, -1));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
            break;
        }
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GenericCommandDispatcher --
 *
 *		Supposed to be a generic command dispatcher.  
 *
 * Results:
 *		Standard Tcl result
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

#if 0

static CONST char *genericCmds[] = {
    "cget", "configure", "create", "delete", "names",
    (char *) NULL
};

enum {
	kPathGenericCmdCget						= 0L,
    kPathGenericCmdConfigure,
    kPathGenericCmdCreate,
    kPathGenericCmdDelete,
    kPathGenericCmdNames
};

int 
GenericCommandDispatcher( 
        ClientData clientData,
        Tcl_Interp* interp,
        char *baseName,
        int *baseNameUIDPtr,
        Tcl_HashTable *hashTablePtr,
        Tk_OptionTable optionTable,
        char *recordPtr,
        int *createAndConfigProc(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]),
        //void *freeProc(),
        int objc,
      	Tcl_Obj* CONST objv[] )
{
    char *name;
    int result = TCL_OK;
    int index;
    Tcl_HashEntry *hPtr;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], genericCmds, "command", 0,
            &index) != TCL_OK) {
        return TCL_ERROR;
    }

    switch (index) {
    
        case kPathGenericCmdCget: {
            Tcl_Obj *resultObjPtr = NULL;
            
    		if (objc != 4) {
				Tcl_WrongNumArgs( interp, 3, objv, "option" );
    			return TCL_ERROR;
    		}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(hashTablePtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, 
                        "object \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            recordPtr = Tcl_GetHashValue(hPtr);
			resultObjPtr = Tk_GetOptionValue(interp, recordPtr, optionTable, objv[3], NULL);
			if (resultObjPtr == NULL) {
				result = TCL_ERROR;
			} else {
				Tcl_SetObjResult( interp, resultObjPtr );
			}
            break;
        }
        
        case kPathGenericCmdConfigure: {
			Tcl_Obj *resultObjPtr = NULL;

			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(hashTablePtr, name);
            if (hPtr == NULL) {
                Tcl_AppendResult(interp, 
                        "object \"", name, "\" doesn't exist", NULL);
                return TCL_ERROR;
            }
            recordPtr = Tcl_GetHashValue(hPtr);
			if (objc <= 4) {
				resultObjPtr = Tk_GetOptionInfo(interp, recordPtr,
                        optionTable,
                        (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
                        NULL);
				if (resultObjPtr == NULL) {
					return TCL_ERROR;
                }
				Tcl_SetObjResult(interp, resultObjPtr);
			} else {
				if (Tk_SetOptions(interp, recordPtr, optionTable, objc - 3, objv + 3, 
                        NULL, NULL, NULL) != TCL_OK) {
					return TCL_ERROR;
                }
			}
            break;
        
        case kPathGenericCmdCreate: {
        
            /*
             * Create with auto generated unique name.
             */
			char str[255];
			int isNew;

			if (objc < 2) {
				Tcl_WrongNumArgs(interp, 2, objv, "?option value...?");
				return TCL_ERROR;
			}
            sprintf(str, "%s%d", baseName, *baseNameUIDPtr);
            *baseNameUIDPtr++;
			stylePtr = *createAndConfigProc(interp, str, objc - 2, objv + 2);
			if (stylePtr == NULL) {
				return TCL_ERROR;
            }
			hPtr = Tcl_CreateHashEntry(hashTablePtr, str, &isNew);
			Tcl_SetHashValue(hPtr, stylePtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(str, -1));
            break;
        }
        }
        
        case -99: {
            /*
             * Create using named style. I don't like this.
             */
			int len;
			int isNew;

			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[2], &len);
			if (!len) {
				FormatResult(interp, "invalid style name \"\"");
				return TCL_ERROR;
			}
			hPtr = Tcl_FindHashEntry(hashTablePtr, name);
			if (hPtr != NULL) {
				FormatResult(interp, "style \"%s\" already exists", name);
				return TCL_ERROR;
			}
			stylePtr = LinGradientCreateAndConfig(interp, name, objc - 3, objv + 3);
			if (stylePtr == NULL) {
				return TCL_ERROR;
            }
			hPtr = Tcl_CreateHashEntry(hashTablePtr, name, &isNew);
			Tcl_SetHashValue(hPtr, stylePtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));
            break;
        }
        
        case kPathGenericCmdDelete: {
			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name");
				return TCL_ERROR;
			}
            name = Tcl_GetString(obj);
            hPtr = Tcl_FindHashEntry(hashTablePtr, name);
			if (hPtr != NULL) {
                Tcl_DeleteHashEntry(hPtr);
			}

			break;
        }
        
        case kPathGenericCmdNames: {
			Tcl_Obj *listObj;
			Tcl_HashSearch search;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
			while (hPtr != NULL) {
                stylePtr = (LinearGradientStyle *) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj, 
                        Tcl_NewStringObj(stylePtr->name, -1));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
            break;
        }
    }
    return result;
}
#endif

void
PathPaintLinearGradientFromName(Drawable d, PathRect *bbox, char *name, int fillRule)
{
    LinearGradientFill *fillPtr;

    if (GetLinearGradientFromName(name, &fillPtr) != TCL_OK) {
        return;
    }
    TkPathPaintLinearGradient(d, bbox, fillPtr, fillRule);
}



