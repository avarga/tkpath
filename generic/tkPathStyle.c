/*
 * tkPathStyle.c --
 *
 *	    This file implements style objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * Note: It would be best to have this in the canvas widget as a special
 *       object, but I see no way of doing this without touching
 *       the canvas code.
 *
 * Note: When a style object is modified or destroyed the corresponding
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

static Tcl_HashTable 	*gStyleHashPtr;
static Tk_OptionTable 	gStyleOptionTable;
static int 				gStyleNameUid = 0;
static char 			*kStyleNameBase = "pathstyle";


/*
 * Declarationd for functions local to this file.
 */

static char *	StyleCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]);
static void		StyleFree(Tcl_Interp *interp, char *recordPtr);


/*
 * Custom option support.
 */

/*
 * The -fillgradient custom option.
 */
 
static int FillGradientSetOption(
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
    char *name, *new;
    int length;
    Tcl_Obj *valuePtr;
    
    valuePtr = *value;
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
    }
    if (internalPtr != NULL) {
		if (valuePtr != NULL) {
		    name = Tcl_GetStringFromObj(valuePtr, &length);
            if (HaveLinearGradientStyleWithName(name) != TCL_OK) {
                Tcl_AppendResult(interp, "bad gradient name \"", name, 
                        "\": does not exist",
                        (char *) NULL);
                return TCL_ERROR;
            }
		    new = ckalloc((unsigned) (length + 1));
		    strcpy(new, name);
		} else {
		    new = NULL;
		}
		*((char **) oldInternalPtr) = *((char **) internalPtr);
		*((char **) internalPtr) = new;
    }
    return TCL_OK;
}

static Tcl_Obj *
FillGradientGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset)		/* Offset within *recordPtr containing the
                             * value. */
{
    char *internalPtr;

    internalPtr = recordPtr + internalOffset;
    return Tcl_NewStringObj(*((char **) internalPtr), -1);
}

static Tk_ObjCustomOption fillGradientCO = 
{
    "fillgradient",
    FillGradientSetOption,
    FillGradientGetOption,
    NULL,
    NULL,			/* We would perhaps need this to be able to
                     * free up the record. */
    (ClientData) NULL
};

/*
 * The -matrix custom option.
 */

static int MatrixSetOption(
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
    char *new;
    char *list;
    int length;
    Tcl_Obj *valuePtr;
    TMatrix *matrixPtr;
    
    valuePtr = *value;
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(valuePtr)) {
		valuePtr = NULL;
    }
    if (internalPtr != NULL) {
		if (valuePtr != NULL) {
            list = Tcl_GetStringFromObj(valuePtr, &length);
            matrixPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
            if (PathGetTMatrix(interp, list, matrixPtr) != TCL_OK) {
                ckfree((char *) matrixPtr);
                return TCL_ERROR;
            }
		    new = matrixPtr;
		} else {
		    new = NULL;
        }
		*((char **) oldInternalPtr) = *((char **) internalPtr);
		*((char **) internalPtr) = new;
    }
    return TCL_OK;
}

static Tcl_Obj *
MatrixGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset)		/* Offset within *recordPtr containing the
                             * value. */
{
    char 		*internalPtr;
    TMatrix 	*matrixPtr;
    Tcl_Obj 	*listObj;
    
    /* @@@ An alternative to this could be to have an objOffset in option table. */

    internalPtr = recordPtr + internalOffset;
    matrixPtr = *((TMatrix **) internalPtr);
    PathGetTclObjFromTMatrix(NULL, matrixPtr, &listObj);
    
    return listObj;
}

static void
MatrixRestoreOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    /* @@@ TODO */
}

static void
MatrixFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    /* @@@ TODO */
}

static Tk_ObjCustomOption matrixCO = 
{
    "matrix",
    MatrixSetOption,
    MatrixGetOption,
    NULL,
    NULL,
    (ClientData) NULL
};

/*
 * The -strokedasharray custom option.
 */

static int DashSetOption(
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
    char *string;
    
    if (internalOffset >= 0) {
        internalPtr = recordPtr + internalOffset;
    } else {
        internalPtr = NULL;
    }
    string = Tcl_GetStringFromObj(*value, NULL);
    return Tk_GetDash(interp, string, (Tk_Dash *) internalPtr);
}

static Tcl_Obj *
DashGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset)		/* Offset within *recordPtr containing the
                             * value. */
{
    Tk_Dash *dash = (Tk_Dash *) (recordPtr + internalOffset);
    char *buffer;
    char *p;
    int i = dash->number;

    if (i < 0) {
        i = -i;
        buffer = (char *) ckalloc((unsigned int) (i+1));
        p = (i > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
        memcpy(buffer, p, (unsigned int) i);
        buffer[i] = 0;
    } else if (!i) {
        buffer = (char *) ckalloc(1);
        buffer[0] = '\0';
    } else {
        buffer = (char *)ckalloc((unsigned int) (4*i));
        p = (i > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
        sprintf(buffer, "%d", *p++ & 0xff);
        while(--i) {
            sprintf(buffer+strlen(buffer), " %d", *p++ & 0xff);
        }
    }
    return Tcl_NewStringObj(buffer, -1);
}

static Tk_ObjCustomOption dashCO = 
{
    "dash",
    DashSetOption,
    DashGetOption,
    NULL,
    NULL,
    (ClientData) NULL
};

/* 
 * These must be kept in sync with defines in X.h! 
 */
static char *fillRuleST[] = {
    "evenodd", "nonzero", (char *) NULL
};
static char *lineCapST[] = {
    "notlast", "butt", "round", "projecting", (char *) NULL
};
static char *lineJoinST[] = {
    "miter", "round", "bevel", (char *) NULL
};

/* Just a placeholder for not yet implemented stuff. */
static char *nullST[] = {
    "", (char *) NULL
};

static Tk_OptionSpec styleOptionSpecs[] = {
    {TK_OPTION_COLOR, "-fill", (char *) NULL, (char *) NULL,
        "", -1, Tk_Offset(Tk_PathStyle, fillColor), TK_OPTION_NULL_OK, 0, 0},
	{TK_OPTION_CUSTOM, "-fillgradient", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, gradientFillName),
		TK_OPTION_NULL_OK, (ClientData) &fillGradientCO, 0},
	{TK_OPTION_STRING_TABLE, "-filloffset", (char *) NULL, (char *) NULL,	/* @@@ TODO */
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, null),
		0, (ClientData) &nullST, 0},
    {TK_OPTION_DOUBLE, "-fillopacity", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, fillOpacity), 0, 0, 0},
    {TK_OPTION_STRING_TABLE, "-fillrule", (char *) NULL, (char *) NULL,
        "nonzero", -1, Tk_Offset(Tk_PathStyle, fillRule), 
        0, (ClientData) fillRuleST, 0},
    {TK_OPTION_BITMAP, "-fillstipple", (char *) NULL, (char *) NULL,
        "", -1, Tk_Offset(Tk_PathStyle, fillStipple), TK_OPTION_NULL_OK, 0, 0},
	{TK_OPTION_CUSTOM, "-matrix", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, matrix),
		TK_OPTION_NULL_OK, (ClientData) &matrixCO, 0},
    {TK_OPTION_COLOR, "-stroke", (char *) NULL, (char *) NULL,
        "black", -1, Tk_Offset(Tk_PathStyle, strokeColor), TK_OPTION_NULL_OK, 0, 0},
	{TK_OPTION_CUSTOM, "-strokedasharray", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, dash),
		TK_OPTION_NULL_OK, (ClientData) &dashCO, 0},
    {TK_OPTION_STRING_TABLE, "-strokelinecap", (char *) NULL, (char *) NULL,
        "butt", -1, Tk_Offset(Tk_PathStyle, capStyle), 
        0, (ClientData) lineCapST, 0},
    {TK_OPTION_STRING_TABLE, "-strokelinejoin", (char *) NULL, (char *) NULL,
        "round", -1, Tk_Offset(Tk_PathStyle, joinStyle), 
        0, (ClientData) lineJoinST, 0},
    {TK_OPTION_DOUBLE, "-strokemiterlimit", (char *) NULL, (char *) NULL,
        "4.0", -1, Tk_Offset(Tk_PathStyle, miterLimit), 0, 0, 0},
	{TK_OPTION_STRING_TABLE, "-strokeoffset", (char *) NULL, (char *) NULL,	/* @@@ TODO */
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, null),
		0, (ClientData) &nullST, 0},
    {TK_OPTION_DOUBLE, "-strokeopacity", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, strokeOpacity), 0, 0, 0},
    {TK_OPTION_BITMAP, "-strokestipple", (char *) NULL, (char *) NULL,
        "", -1, Tk_Offset(Tk_PathStyle, strokeStipple), TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_DOUBLE, "-strokewidth", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, strokeWidth), 0, 0, 0},
    
    /* @@@ TODO: When this comes into canvas code we should add a -tags option here??? */
    
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};


void
PathStyleInit(Tcl_Interp* interp) 
{
    gStyleHashPtr = (Tcl_HashTable *) ckalloc( sizeof(Tcl_HashTable) );
    Tcl_InitHashTable(gStyleHashPtr, TCL_STRING_KEYS);
    
    /*
     * The option table must only be made once and not for each instance.
     */
    gStyleOptionTable = Tk_CreateOptionTable(interp, styleOptionSpecs);
}

/*
 *----------------------------------------------------------------------
 *
 * StyleObjCmd --
 *
 *		This implements the tkpath::style command.  
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
StyleObjCmd( 
        ClientData clientData,
        Tcl_Interp* interp,
        int objc,
      	Tcl_Obj* CONST objv[] )
{
    return PathGenericCmdDispatcher(interp, objc, objv, 
            kStyleNameBase, &gStyleNameUid, gStyleHashPtr, gStyleOptionTable,
            StyleCreateAndConfig, StyleFree);
}


static char *
StyleCreateAndConfig(
        Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[])
{
	Tk_PathStyle *stylePtr;
    Tk_Window tkwin = Tk_MainWindow(interp); /* Should have been the canvas. */

	stylePtr = (Tk_PathStyle *) ckalloc(sizeof(Tk_PathStyle));
	memset(stylePtr, '\0', sizeof(Tk_PathStyle));
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	stylePtr->optionTable = gStyleOptionTable; 
	stylePtr->name = Tk_GetUid(name);
    
    /* Fill in defaults */
    Tk_CreatePathStyle(stylePtr);

	if (Tk_InitOptions(interp, (char *) stylePtr, stylePtr->optionTable, 
            tkwin) != TCL_OK) {
		ckfree((char *) stylePtr);
		return NULL;
	}

	if (Tk_SetOptions(interp, (char *) stylePtr, stylePtr->optionTable, 	
            objc, objv, tkwin, NULL, NULL) != TCL_OK) {
		Tk_FreeConfigOptions((char *) stylePtr, stylePtr->optionTable, NULL);
		ckfree((char *) stylePtr);
		return NULL;
	}

    return (char *) stylePtr;
}

static void
StyleFree(Tcl_Interp *interp, char *recordPtr) 
{
    //Tk_DeletePathStyle(display, (Tk_PathStyle *) recordPtr);
    ckfree(recordPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreatePathStyle
 *
 *	This procedure initializes the Tk_PathStyle structure
 *	with default values.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

void 
Tk_CreatePathStyle(Tk_PathStyle *style)
{
    style->strokeGC = None;
    style->strokeColor = NULL;
    style->strokeWidth = 1.0;
    style->strokeOpacity = 1.0;
    style->offset = 0;
    style->dash.number = 0;
    style->strokeTSOffset.flags = 0;
    style->strokeTSOffset.xoffset = 0;
    style->strokeTSOffset.yoffset = 0;
    style->strokeStipple = None;
    style->capStyle = CapButt;
    style->joinStyle = JoinRound;

    style->fillGC = None;
    style->fillColor = NULL;
    style->fillOpacity = 1.0;
    style->fillTSOffset.flags = 0;
    style->fillTSOffset.xoffset = 0;
    style->fillTSOffset.yoffset = 0;
    style->fillStipple = None;
    style->fillRule = WindingRule;
    style->gradientFillName = NULL;    
    style->matrix = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeletePathStyle
 *
 *	This procedure frees all memory that might be
 *	allocated and referenced in the Tk_PathStyle structure.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

void 
Tk_DeletePathStyle(Display *display, Tk_PathStyle *style)
{
    if (style->strokeGC != None) {
        Tk_FreeGC(display, style->strokeGC);
    }
    if (ABS(style->dash.number) > sizeof(char *)) {
        ckfree((char *) style->dash.pattern.pt);
    }
    if (style->strokeColor != NULL) {
        Tk_FreeColor(style->strokeColor);
    }
    if (style->strokeStipple != None) {
        Tk_FreeBitmap(display, style->strokeStipple);
    }

    if (style->fillGC != None) {
        Tk_FreeGC(display, style->fillGC);
    }
    if (style->fillColor != NULL) {
        Tk_FreeColor(style->fillColor);
    }
    if (style->fillStipple != None) {
        Tk_FreeBitmap(display, style->fillStipple);
    }
    if (style->matrix != NULL) {
        ckfree((char *) style->matrix);
    }
}

/*-------------------------------------------------------------------*/






