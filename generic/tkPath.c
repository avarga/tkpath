/*
 * tkPath.h --
 *
 *		This file implements a path drawing model
 *      SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *		It contains the generic parts that do not refer to the canvas.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <tkInt.h>
#include "tkPath.h"
#include "tkIntPath.h"



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
    style->matrixStr = NULL;
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

    if (style->matrixStr != NULL) {
	ckfree((char *) style->matrixStr);
    }
    if (style->matrix != NULL) {
	ckfree((char *) style->matrix);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetPathInstruction --
 *
 *	Gets the path instruction at position index of objv.
 *	If unrecognized instruction returns PATH_NEXT_ERROR.
 *
 * Results:
 *	A PATH_NEXT_* result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetPathInstruction(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int index, char *c) 
{
    int len;
    int result;
    char *str;
    
    *c = '\0';
    str = Tcl_GetStringFromObj(objv[index], &len);
    if (isalpha(str[0])) {
        if (len != 1) {
            result = PATH_NEXT_ERROR;
        } else {
            switch (str[0]) {
                case 'M': case 'm': case 'L': case 'l':
                case 'H': case 'h': case 'V': case 'v':
                case 'A': case 'a': case 'Q': case 'q':
                case 'T': case 't': case 'C': case 'c':
                case 'S': case 's': case 'Z': case 'z':
                    result = PATH_NEXT_INSTRUCTION;
                    *c = str[0];
                    break;
                default:
                    result = PATH_NEXT_ERROR;
                    break;
            }
        }
    } else {
        result = PATH_NEXT_OTHER;
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * GetPathDouble, GetPathBoolean, GetPathPoint, GetPathTwoPoints,
 * GetPathThreePoints, GetPathArcParameters --
 *
 *	Gets a certain number of numbers from objv.
 *	Increments indexPtr by the number of numbers extracted
 *	if succesful, else it is unchanged.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetPathDouble(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr, double *zPtr) 
{
    int result;

    result = Tcl_GetDoubleFromObj(interp, objv[*indexPtr], zPtr);
    if (result == TCL_OK) {
        (*indexPtr)++;
    }
    return result;
}

static int
GetPathBoolean(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr, char *boolPtr) 
{
    int result;
    int bool;

    result = Tcl_GetBooleanFromObj(interp, objv[*indexPtr], &bool);
    if (result == TCL_OK) {
        (*indexPtr)++;
        *boolPtr = bool;
    }
    return result;
}

static int
GetPathPoint(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr, 
        double *xPtr, double *yPtr)
{
    int result = TCL_OK;
    int indIn = *indexPtr;
    
    if (Tcl_GetDoubleFromObj(interp, objv[(*indexPtr)++], xPtr) != TCL_OK) {
        *indexPtr = indIn;
        result = TCL_ERROR;
    } else if (Tcl_GetDoubleFromObj(interp, objv[(*indexPtr)++], yPtr) != TCL_OK) {
        *indexPtr = indIn;
        result = TCL_ERROR;
    }
    return result;
}

static int
GetPathTwoPoints(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr, 
        double *x1Ptr, double *y1Ptr, double *x2Ptr, double *y2Ptr)
{
    int result;
    int indIn = *indexPtr;

    result = GetPathPoint(interp, objv, indexPtr, x1Ptr, y1Ptr);
    if (result == TCL_OK) {
        if (GetPathPoint(interp, objv, indexPtr, x2Ptr, y2Ptr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        }
    }
    return result;
}

static int
GetPathThreePoints(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr, 
        double *x1Ptr, double *y1Ptr, double *x2Ptr, double *y2Ptr,
        double *x3Ptr, double *y3Ptr)
{
    int result;
    int indIn = *indexPtr;

    result = GetPathPoint(interp, objv, indexPtr, x1Ptr, y1Ptr);
    if (result == TCL_OK) {
        if (GetPathPoint(interp, objv, indexPtr, x2Ptr, y2Ptr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        } else if (GetPathPoint(interp, objv, indexPtr, x3Ptr, y3Ptr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        }
    }
    return result;
}

static int
GetPathArcParameters(Tcl_Interp *interp, Tcl_Obj *CONST objv[], int *indexPtr,
        double *radXPtr, double *radYPtr, double *anglePtr, 
        char *largeArcFlagPtr, char *sweepFlagPtr, 
        double *xPtr, double *yPtr)
{
    int result;
    int indIn = *indexPtr;

    result = GetPathPoint(interp, objv, indexPtr, radXPtr, radYPtr);
    if (result == TCL_OK) {
        if (GetPathDouble(interp, objv, indexPtr, anglePtr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        } else if (GetPathBoolean(interp, objv, indexPtr, largeArcFlagPtr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        } else if (GetPathBoolean(interp, objv, indexPtr, sweepFlagPtr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        } else if (GetPathPoint(interp, objv, indexPtr, xPtr, yPtr) != TCL_OK) {
            *indexPtr = indIn;
            result = TCL_ERROR;
        } 
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * NewMoveToAtom, NewLineToAtom, NewArcAtom, NewQuadBezierAtom,
 * NewCurveToAtom, NewCloseAtom --
 *
 *	Creates a PathAtom of the specified type using the given
 *	parameters. It updates the currentX and currentY.
 *
 * Results:
 *	A PathAtom pointer.
 *
 * Side effects:
 *	Memory allocated.
 *
 *--------------------------------------------------------------
 */

static PathAtom *
NewMoveToAtom(double x, double y)
{
    PathAtom *atomPtr;
    MoveToAtom *moveToAtomPtr;

    moveToAtomPtr = (MoveToAtom *) ckalloc((unsigned) (sizeof(MoveToAtom)));
    atomPtr = (PathAtom *) moveToAtomPtr;
    atomPtr->type = PATH_ATOM_M;
    atomPtr->nextPtr = NULL;
    moveToAtomPtr->x = x;
    moveToAtomPtr->y = y;
    return atomPtr;
}

static PathAtom *
NewLineToAtom(double x, double y)
{
    PathAtom *atomPtr;
    LineToAtom *lineToAtomPtr;

    lineToAtomPtr = (LineToAtom *) ckalloc((unsigned) (sizeof(LineToAtom)));
    atomPtr = (PathAtom *) lineToAtomPtr;
    atomPtr->type = PATH_ATOM_L;
    atomPtr->nextPtr = NULL;
    lineToAtomPtr->x = x;
    lineToAtomPtr->y = y;
    return atomPtr;
}

static PathAtom *
NewArcAtom(double radX, double radY, 
        double angle, char largeArcFlag, char sweepFlag, double x, double y)
{
    PathAtom *atomPtr;
    ArcAtom *arcAtomPtr;

    arcAtomPtr = (ArcAtom *) ckalloc((unsigned) (sizeof(ArcAtom)));
    atomPtr = (PathAtom *) arcAtomPtr;
    atomPtr->type = PATH_ATOM_A;
    atomPtr->nextPtr = NULL;    
    arcAtomPtr->radX = radX;
    arcAtomPtr->radY = radY;
    arcAtomPtr->angle = angle;
    arcAtomPtr->largeArcFlag = largeArcFlag;
    arcAtomPtr->sweepFlag = sweepFlag;
    arcAtomPtr->x = x;
    arcAtomPtr->y = y;
    return atomPtr;
}

static PathAtom *
NewQuadBezierAtom(double ctrlX, double ctrlY, double anchorX, double anchorY)
{
    PathAtom *atomPtr;
    QuadBezierAtom *quadBezierAtomPtr;

    quadBezierAtomPtr = (QuadBezierAtom *) ckalloc((unsigned) (sizeof(QuadBezierAtom)));
    atomPtr = (PathAtom *) quadBezierAtomPtr;
    atomPtr->type = PATH_ATOM_Q;
    atomPtr->nextPtr = NULL;
    quadBezierAtomPtr->ctrlX = ctrlX;
    quadBezierAtomPtr->ctrlY = ctrlY;
    quadBezierAtomPtr->anchorX = anchorX;
    quadBezierAtomPtr->anchorY = anchorY;
    return atomPtr;
}

static PathAtom *
NewCurveToAtom(double ctrlX1, double ctrlY1, double ctrlX2, double ctrlY2, 
        double anchorX, double anchorY)
{
    PathAtom *atomPtr;
    CurveToAtom *curveToAtomPtr;

    curveToAtomPtr = (CurveToAtom *) ckalloc((unsigned) (sizeof(CurveToAtom)));
    atomPtr = (PathAtom *) curveToAtomPtr;
    atomPtr->type = PATH_ATOM_C;
    atomPtr->nextPtr = NULL;
    curveToAtomPtr->ctrlX1 = ctrlX1;
    curveToAtomPtr->ctrlY1 = ctrlY1;
    curveToAtomPtr->ctrlX2 = ctrlX2;
    curveToAtomPtr->ctrlY2 = ctrlY2;
    curveToAtomPtr->anchorX = anchorX;
    curveToAtomPtr->anchorY = anchorY;
    return atomPtr;
}

static PathAtom *
NewCloseAtom(double x, double y)
{
    PathAtom *atomPtr;
    CloseAtom *closeAtomPtr;

    closeAtomPtr = (CloseAtom *) ckalloc((unsigned) (sizeof(CloseAtom)));
    atomPtr = (PathAtom *) closeAtomPtr;
    atomPtr->type = PATH_ATOM_Z;
    atomPtr->nextPtr = NULL;
    return atomPtr;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathParseToAtoms
 *
 *	Takes a tcl list of values which defines the path item and
 *	parses them into a linked list of path atoms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

int
TkPathParseToAtoms(Tcl_Interp *interp, Tcl_Obj *listObjPtr, PathAtom **atomPtrPtr, int *lenPtr)
{
    char currentInstr;		/* current instruction (M, l, c, etc.) */
    char lastInstr;		/* previous instruction */
    int len;
    int currentInd;
    int index;
    int next;
    int relative;
    double currentX, currentY;	/* current point */
    double startX, startY;	/* the current moveto point */
    double ctrlX, ctrlY;	/* last control point, for s, S, t, T */
    double x, y;
    Tcl_Obj **objv;
    PathAtom *atomPtr = NULL;
    PathAtom *currentAtomPtr = NULL;
    
    *atomPtrPtr = NULL;
    currentX = 0.0;
    currentY = 0.0;
    ctrlX = 0.0;
    ctrlY = 0.0;
    lastInstr = 'M';	/* If first instruction is missing it defaults to M ? */
        
    if (Tcl_ListObjGetElements(interp, listObjPtr, lenPtr, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    len = *lenPtr;
    
    /* First some error checking. Necessary??? */
    if (len < 3) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"path specification too short", -1));
        return TCL_ERROR;
    }
    if ((GetPathInstruction(interp, objv, 0, &currentInstr) != PATH_NEXT_INSTRUCTION) || 
            (toupper(currentInstr) != 'M')) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"path must start with M or m", -1));
        return TCL_ERROR;
    }
    currentInd = 1;
    if (GetPathPoint(interp, objv, &currentInd, &x, &y) != TCL_OK) {
        return TCL_ERROR;
    }
    currentInd = 0;
     
    while (currentInd < *lenPtr) {

        next = GetPathInstruction(interp, objv, currentInd, &currentInstr);
        if (next == PATH_NEXT_ERROR) {
            goto error;
        } else if (next == PATH_NEXT_INSTRUCTION) {
            relative = islower(currentInstr);
            currentInd++;
        } else if (next == PATH_NEXT_OTHER) {
        
            /* Use rule to find instruction to use. */
            if (lastInstr == 'M') {
                currentInstr = 'L';
            } else if (lastInstr == 'm') {
                currentInstr = 'l';
            } else {
                currentInstr = lastInstr;
            }
            relative = islower(currentInstr);
        }
        index = currentInd;
        
        switch (currentInstr) {
            case 'M': case 'm': {
                if (GetPathPoint(interp, objv, &index, &x, &y) != TCL_OK) {
                    goto error;
                }
                if (relative) {
                    x += currentX;
                    y += currentY;
                }    
                atomPtr = NewMoveToAtom(x, y);
                if (currentAtomPtr == NULL) {
                    *atomPtrPtr = atomPtr;
                } else {
                    currentAtomPtr->nextPtr = atomPtr;
                }
                currentAtomPtr = atomPtr;
                currentX = x;
                currentY = y;
                startX = x;
                startY = y;
                break;
            }
            case 'L': case 'l': {
                if (GetPathPoint(interp, objv, &index, &x, &y) == TCL_OK) {
                    if (relative) {
                        x += currentX;
                        y += currentY;
                    }    
                    atomPtr = NewLineToAtom(x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'A': case 'a': {
                double radX, radY, angle;
                char largeArcFlag, sweepFlag;
                
                if (GetPathArcParameters(interp, objv, &index,
                        &radX, &radY, &angle, &largeArcFlag, &sweepFlag,
                        &x, &y) == TCL_OK) {
                    if (relative) {
                        x += currentX;
                        y += currentY;
                    }    
                    atomPtr = NewArcAtom(radX, radY, angle, largeArcFlag, sweepFlag, x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'C': case 'c': {
                double x1, y1, x2, y2;	/* The two control points. */
                
                if (GetPathThreePoints(interp, objv, &index, &x1, &y1, &x2, &y2, &x, &y) == TCL_OK) {
                    if (relative) {
                        x1 += currentX;
                        y1 += currentY;
                        x2 += currentX;
                        y2 += currentY;
                        x  += currentX;
                        y  += currentY;
                    }    
                    atomPtr = NewCurveToAtom(x1, y1, x2, y2, x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    ctrlX = x2; 	/* Keep track of the last control point. */
                    ctrlY = y2;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'S': case 's': {
                double x1, y1;	/* The first control point. */
                double x2, y2;	/* The second control point. */
                
                if ((toupper(lastInstr) == 'C') || (toupper(lastInstr) == 'S')) {
                    /* The first controlpoint is the reflection of the last one about the current point: */
                    x1 = 2 * currentX - ctrlX;
                    y1 = 2 * currentY - ctrlY;                    
                } else {
                    /* The first controlpoint is equal to the current point: */
                    x1 = currentX;
                    y1 = currentY;
                }
                if (GetPathTwoPoints(interp, objv, &index, &x2, &y2, &x, &y) == TCL_OK) {
                    if (relative) {
                        x2 += currentX;
                        y2 += currentY;
                        x  += currentX;
                        y  += currentY;
                    }    
                    atomPtr = NewCurveToAtom(x1, y1, x2, y2, x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    ctrlX = x2; 	/* Keep track of the last control point. */
                    ctrlY = y2;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'Q': case 'q': {
                double x1, y1;	/* The control point. */
                
                if (GetPathTwoPoints(interp, objv, &index, &x1, &y1, &x, &y) == TCL_OK) {
                    if (relative) {
                        x1 += currentX;
                        y1 += currentY;
                        x  += currentX;
                        y  += currentY;
                    }    
                    atomPtr = NewQuadBezierAtom(x1, y1, x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    ctrlX = x1; 	/* Keep track of the last control point. */
                    ctrlY = y1;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'T': case 't': {
                double x1, y1;	/* The control point. */
                
                if ((toupper(lastInstr) == 'Q') || (toupper(lastInstr) == 'T')) {
                    /* The controlpoint is the reflection of the last one about the current point: */
                    x1 = 2 * currentX - ctrlX;
                    y1 = 2 * currentY - ctrlY;                    
                } else {
                    /* The controlpoint is equal to the current point: */
                    x1 = currentX;
                    y1 = currentY;
                }
                if (GetPathPoint(interp, objv, &index, &x, &y) == TCL_OK) {
                    if (relative) {
                        x  += currentX;
                        y  += currentY;
                    }    
                    atomPtr = NewQuadBezierAtom(x1, y1, x, y);
                    currentAtomPtr->nextPtr = atomPtr;
                    currentAtomPtr = atomPtr;
                    ctrlX = x1; 	/* Keep track of the last control point. */
                    ctrlY = y1;
                    currentX = x;
                    currentY = y;
                } else {
                    goto error;
                }
                break;
            }
            case 'H': {
                while ((index < len) && 
                        (GetPathDouble(interp, objv, &index, &x) == TCL_OK))
                    ;
                atomPtr = NewLineToAtom(x, currentY);
                currentAtomPtr->nextPtr = atomPtr;
                currentAtomPtr = atomPtr;
                currentX = x;
                break;
            }
            case 'h': {
                double z;
                
                x = currentX;
                while ((index < len) &&
                        (GetPathDouble(interp, objv, &index, &z) == TCL_OK)) {
                    x += z;
                }
                atomPtr = NewLineToAtom(x, currentY);
                currentAtomPtr->nextPtr = atomPtr;
                currentAtomPtr = atomPtr;
                currentX = x;
                break;
            }
            case 'V': {
                while ((index < len) && 
                        (GetPathDouble(interp, objv, &index, &y) == TCL_OK))
                    ;
                atomPtr = NewLineToAtom(currentX, y);
                currentAtomPtr->nextPtr = atomPtr;
                currentAtomPtr = atomPtr;
                currentY = y;
                break;
            }
            case 'v': {
                double z;
                
                y = currentY;
                while ((index < len) &&
                        (GetPathDouble(interp, objv, &index, &z) == TCL_OK)) {
                    y += z;
                }
                atomPtr = NewLineToAtom(currentX, y);
                currentAtomPtr->nextPtr = atomPtr;
                currentAtomPtr = atomPtr;
                currentY = y;
                break;
            }
            case 'Z': case 'z': {
                atomPtr = NewCloseAtom(startX, startY);
                currentAtomPtr->nextPtr = atomPtr;
                currentAtomPtr = atomPtr;
                currentX = startX;
                currentY = startY;
                break;
            }
            default: {
                Tcl_SetObjResult(interp, Tcl_NewStringObj(
                        "unrecognized path instruction", -1));
                goto error;
            }
        }
        currentInd = index;
        lastInstr = currentInstr;
    }
    
    /* When we parse coordinates there may be some junk result
     * left in the interpreter to be cleared out. */
    Tcl_ResetResult(interp);
    return TCL_OK;
    
error:

    TkPathFreeAtoms(*atomPtrPtr);
    *atomPtrPtr = NULL;
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathFreeAtoms
 *
 *	Frees up all memory allocated for the path atoms.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TkPathFreeAtoms(PathAtom *pathAtomPtr)
{
    PathAtom *tmpAtomPtr;

    while (pathAtomPtr != NULL) {
        tmpAtomPtr = pathAtomPtr;
        pathAtomPtr = tmpAtomPtr->nextPtr;
        ckfree((char *) tmpAtomPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkPathNormalize
 *
 *	Takes a list of PathAtoms and creates a tcl list where
 *	elements have a standard form. All upper case instructions,
 *	no repeates.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	New list returned in listObjPtrPtr.
 *
 *--------------------------------------------------------------
 */

int
TkPathNormalize(Tcl_Interp *interp, PathAtom *atomPtr, Tcl_Obj **listObjPtrPtr)
{
    Tcl_Obj *normObjPtr;    

    normObjPtr = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );

    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: { 
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("M", -1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(move->x));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(move->y));
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("L", -1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(line->x));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(line->y));
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("A", -1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(arc->radX));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(arc->radY));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(arc->angle));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewBooleanObj(arc->largeArcFlag));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewBooleanObj(arc->sweepFlag));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(arc->x));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(arc->y));
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("Q", -1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(quad->ctrlX));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(quad->ctrlY));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(quad->anchorX));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(quad->anchorY));
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;

                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("C", -1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->ctrlX1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->ctrlY1));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->ctrlX2));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->ctrlY2));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->anchorX));
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewDoubleObj(curve->anchorY));
                break;
            }
            case PATH_ATOM_Z: {
                Tcl_ListObjAppendElement(interp, normObjPtr, Tcl_NewStringObj("Z", -1));
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    *listObjPtrPtr = normObjPtr;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathMakePath
 *
 *	Defines the path using the PathAtom.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines the current path in drawable.
 *
 *--------------------------------------------------------------
 */

int
TkPathMakePath(
    Drawable drawable,			/* Pixmap or window in which to draw
					 * item. */
    PathAtom *atomPtr,
    Tk_PathStyle *stylePtr)
{
    TkPathBeginPath(drawable, stylePtr);

    while (atomPtr != NULL) {
    
        switch (atomPtr->type) {
            case PATH_ATOM_M: { 
                MoveToAtom *move = (MoveToAtom *) atomPtr;
                
                TkPathMoveTo(drawable, move->x, move->y);
                break;
            }
            case PATH_ATOM_L: {
                LineToAtom *line = (LineToAtom *) atomPtr;
                
                TkPathLineTo(drawable, line->x, line->y);
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atomPtr;
                
                TkPathArcTo(drawable, arc->radX, arc->radY, arc->angle, 
                        arc->largeArcFlag, arc->sweepFlag,
                        arc->x, arc->y);
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atomPtr;
                
                TkPathQuadBezier(drawable, 
                        quad->ctrlX, quad->ctrlY,
                        quad->anchorX, quad->anchorY);
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atomPtr;
                
                TkPathCurveTo(drawable, 
                        curve->ctrlX1, curve->ctrlY1,
                        curve->ctrlX2, curve->ctrlY2,
                        curve->anchorX, curve->anchorY);
                break;
            }
            case PATH_ATOM_Z: {
                TkPathClosePath(drawable);
                break;
            }
        }
        atomPtr = atomPtr->nextPtr;
    }
    TkPathEndPath(drawable);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathArcToUsingBezier
 *
 *	Translates an ArcTo drawing into a sequence of CurveTo.
 *	Helper function for the platform specific drawing code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TkPathArcToUsingBezier(Drawable d,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x2, double y2)
{
    int result;
    int i, segments;
    double x1, y1;
    double cx, cy;
    double theta1, dtheta, phi;
    double sinPhi, cosPhi;
    double delta, t;
    PathPoint pt;
    
    TkPathGetCurrentPosition(d, &pt);
    x1 = pt.x;
    y1 = pt.y;

    /* All angles except phi is in radians! */
    phi = phiDegrees * DEGREES_TO_RADIANS;
    
    /* Check return value and take action. */
    result = EndpointToCentralArcParameters(x1, y1,
            x2, y2, rx, ry, phi, largeArcFlag, sweepFlag,
            &cx, &cy, &rx, &ry,
            &theta1, &dtheta);
	if (result == kPathArcSkip) {
		return;
	} else if (result == kPathArcLine) {
		TkPathLineTo(d, x2, y2);
		return;
    }
    sinPhi = sin(phi);
    cosPhi = cos(phi);
    
    /* Convert into cubic bezier segments <= 90deg (from mozilla/svg; not checked) */
    segments = (int) ceil(fabs(dtheta/(PI/2.0)));
    delta = dtheta/segments;
    t = 8.0/3.0 * sin(delta/4.0) * sin(delta/4.0) / sin(delta/2.0);
    
    for (i = 0; i < segments; ++i) {
        double cosTheta1 = cos(theta1);
        double sinTheta1 = sin(theta1);
        double theta2 = theta1 + delta;
        double cosTheta2 = cos(theta2);
        double sinTheta2 = sin(theta2);
        
        /* a) calculate endpoint of the segment: */
        double xe = cosPhi * rx*cosTheta2 - sinPhi * ry*sinTheta2 + cx;
        double ye = sinPhi * rx*cosTheta2 + cosPhi * ry*sinTheta2 + cy;
    
        /* b) calculate gradients at start/end points of segment: */
        double dx1 = t * ( - cosPhi * rx*sinTheta1 - sinPhi * ry*cosTheta1);
        double dy1 = t * ( - sinPhi * rx*sinTheta1 + cosPhi * ry*cosTheta1);
        
        double dxe = t * ( cosPhi * rx*sinTheta2 + sinPhi * ry*cosTheta2);
        double dye = t * ( sinPhi * rx*sinTheta2 - cosPhi * ry*cosTheta2);
    
        /* c) draw the cubic bezier: */
        TkPathCurveTo(d, x1+dx1, y1+dy1, xe+dxe, ye+dye, xe, ye);

        /* do next segment */
        theta1 = theta2;
        x1 = (float) xe;
        y1 = (float) ye;
    }
}

static int
DashConvertToFloats (
    float *d,		/* The resulting dashes. (Out) */	
    CONST char *p,	/* A string of "_-,." */
    size_t n,
    double width)
{
    int result = 0;
    int size;

    if (n < 0) {
	n = strlen(p);
    }
    while (n-- && *p) {
	switch (*p++) {
	    case ' ':
		if (result) {
		    if (d) {
			d[-1] += (float) (width) + 1.0;
		    }
		    continue;
		} else {
		    return 0;
		}
		break;
	    case '_':
		size = 8;
		break;
	    case '-':
		size = 6;
		break;
	    case ',':
		size = 4;
		break;
	    case '.':
		size = 2;
		break;
	    default:
		return -1;
	}
	if (d) {
	    *d++ = size * (float) width;
	    *d++ = 4 * (float) width;
	}
	result += 2;
    }
    return result;
}

void
PathParseDashToArray(Tk_Dash *dash, double width, int *len, float **arrayPtrPtr)
{    
    char *pt;
    int	i;
    float *arrPtr = NULL;

    if (dash->number == 0) {
        *len = 0;
    } else if (dash->number < 0) {
        
        /* Any of . , - _ verbatim. */
        i = -1*dash->number;
        pt = (i > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
        arrPtr = (float *) ckalloc(2*i*sizeof(float));
	i = DashConvertToFloats(arrPtr, pt, i, width);
        if (i < 0) {
            /* This should never happen since syntax already checked. */
            *len = 0;
        } else {
            *len = i;
        }
    } else {
        pt = (dash->number > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
        *len = dash->number;
        arrPtr = (float *) ckalloc(dash->number * sizeof(float));
        for (i = 0; i < dash->number; i++) {
        
            /* We could optionally multiply with 'width' here. */
            arrPtr[i] = pt[i];
        }
    }
    *arrayPtrPtr = arrPtr;
}


static double 
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
    return ((ux*vy-uy*vx) > 0.0 ? 1.0 : -1.0) *
            acos( (ux*vx+uy*vy)/(hypot(ux,uy)*hypot(vx,vy)));
}

/* from mozilla */
static double 
CalcVectorAngle2(double ux, double uy, double vx, double vy)
{
    double ta = atan2(uy, ux);
    double tb = atan2(vy, vx);
    if (tb >= ta)
        return tb-ta;
    return 2.0*PI - (ta-tb);
}

/*
 *--------------------------------------------------------------
 *
 * CentralToEndpointArcParameters
 *
 *	Conversion from center to endpoint parameterization.
 *	All angles in radians!
 *	From: http://www.w3.org/TR/2003/REC-SVG11-20030114
 *
 * Results:
 *	Arc specific return code.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
CentralToEndpointArcParameters(
        double cx, double cy, double rx, double ry,	/* In pars. */
        double theta1, double dtheta, double phi,
        double *x1Ptr, double *y1Ptr, 			/* Out. */
        double *x2Ptr, double *y2Ptr, 
        char *largeArcFlagPtr, char *sweepFlagPtr)	
{
    double theta2;
    double sinPhi, cosPhi;
    double sinTheta1, cosTheta1;
    double sinTheta2, cosTheta2;

    theta2 = theta1 + dtheta;
    sinPhi = sin(phi);
    cosPhi = cos(phi);
    sinTheta1 = sin(theta1);
    cosTheta1 = cos(theta1);
    sinTheta2 = sin(theta2);
    cosTheta2 = cos(theta2);
    
    /* F.6.4 Conversion from center to endpoint parameterization. */
    *x1Ptr = cx + rx * cosTheta1 * cosPhi - ry * sinTheta1 * sinPhi;
    *y1Ptr = cy + rx * cosTheta1 * sinPhi + ry * sinTheta1 * cosPhi;
    *x2Ptr = cx + rx * cosTheta2 * cosPhi - ry * sinTheta2 * sinPhi;
    *y2Ptr = cy + rx * cosTheta2 * sinPhi + ry * sinTheta2 * cosPhi;

    *largeArcFlagPtr = (dtheta > PI) ? 1 : 0;
    *sweepFlagPtr = (dtheta > 0.0) ? 1 : 0;

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * EndpointToCentralArcParameters
 *
 *	Conversion from endpoint to center parameterization.
 *	All angles in radians!
 *	From: http://www.w3.org/TR/2003/REC-SVG11-20030114
 *
 * Results:
 *	Arc specific return code.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
EndpointToCentralArcParameters(
        double x1, double y1, double x2, double y2,	/* The endpoints. */
        double rx, double ry,				/* Radius. */
        double phi, char largeArcFlag, char sweepFlag,
        double *cxPtr, double *cyPtr, 			/* Out. */
        double *rxPtr, double *ryPtr,
        double *theta1Ptr, double *dthetaPtr)
{
    double sinPhi, cosPhi;
    double dx, dy;
    double x1dash, y1dash;
    double cxdash, cydash;
    double cx, cy;
    double numerator, lambda, root;
    double theta1, dtheta;

    /* 1. Treat out-of-range parameters as described in
     * http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
     *
     * If the endpoints (x1, y1) and (x2, y2) are identical, then this
     * is equivalent to omitting the elliptical arc segment entirely
     */
    if (x1 == x2 && y1 == y2) {
        return kPathArcSkip;
    }
    
    /* If rX = 0 or rY = 0 then this arc is treated as a straight line
     * segment (a "lineto") joining the endpoints.
     */
    if (rx == 0.0f || ry == 0.0f) {
        return kPathArcLine;
    }

    /* If rx or ry have negative signs, these are dropped; the absolute
     * value is used instead.
     */
    if (rx < 0.0) rx = -rx;
    if (ry < 0.0) ry = -ry;

    if (largeArcFlag != 0) largeArcFlag = 1;
    if (sweepFlag != 0) sweepFlag = 1;
  
    /* 2. convert to center parameterization as shown in
     * http://www.w3.org/TR/SVG/implnote.html
     */
    sinPhi = sin(phi);
    cosPhi = cos(phi);
    dx = (x1-x2)/2.0;
    dy = (y1-y2)/2.0;
    x1dash =  cosPhi * dx + sinPhi * dy;
    y1dash = -sinPhi * dx + cosPhi * dy;

    /* F.6.6 Correction of out-of-range radii. */
    lambda = x1dash*x1dash/(rx*rx) + y1dash*y1dash/(ry*ry);
    if (lambda > 1.0) {
        double sqrtLambda;
        
        sqrtLambda = sqrt(lambda);
        rx = sqrtLambda * rx;
        ry = sqrtLambda *ry;
    }    

    /* Compute cx' and cy'. */
    numerator = rx*rx*ry*ry - rx*rx*y1dash*y1dash - ry*ry*x1dash*x1dash;
    root = (largeArcFlag == sweepFlag ? -1.0 : 1.0) *
            sqrt( numerator/(rx*rx*y1dash*y1dash+ry*ry*x1dash*x1dash) );
    cxdash =  root*rx*y1dash/ry;
    cydash = -root*ry*x1dash/rx;

    /* Compute cx and cy from cx' and cy'. */
    cx = cosPhi * cxdash - sinPhi * cydash + (x1+x2)/2.0;
    cy = sinPhi * cxdash + cosPhi * cydash + (y1+y2)/2.0;

    /* Compute start angle and extent. */
    theta1 = CalcVectorAngle(1.0, 0.0, (x1dash-cxdash)/rx, (y1dash-cydash)/ry);
    dtheta = CalcVectorAngle(
            (x1dash-cxdash)/rx,  (y1dash-cydash)/ry,
            (-x1dash-cxdash)/rx, (-y1dash-cydash)/ry);
    if (sweepFlag == 0 && dtheta > 0.0) {
        dtheta -= 2.0*PI;
    } else if (sweepFlag == 1 && dtheta < 0.0) {
        dtheta += 2.0*PI;
    }
    *cxPtr = cx;
    *cyPtr = cy;
    *rxPtr = rx; 
    *ryPtr = ry;
    *theta1Ptr = theta1;
    *dthetaPtr = dtheta; 
    
    return kPathArcOK;
}

/*
 *--------------------------------------------------------------
 *
 * TableLooup
 *
 *	Look up an index from a statically allocated table of ints.
 *
 * Results:
 *	integer
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */

int 
TableLookup(LookupTable *map, int n, int from)
{
    int i = 0;
    
    while ((i < n) && (from != map[i].from))
        i++;
    if (i == n) {
        return map[0].to;
    } else {
        return map[i].to;
    }
}

void 
PathApplyTMatrix(TMatrix *m, double *x, double *y)
{
    double tmpx = *x;
    double tmpy = *y;
    *x = tmpx*m->a + tmpy*m->c + m->tx;
    *y = tmpx*m->b + tmpy*m->d + m->ty;
}

void
PathInverseTMatrix(TMatrix *m, TMatrix *mi)
{
    double det;
    
    det = m->a * m->d - m->b * m->c;
    mi->a  =  m->d/det;
    mi->b  = -m->b/det;
    mi->c  = -m->c/det;
    mi->d  =  m->a/det;
    mi->tx = (m->c * m->ty - m->d * m->tx)/det;
    mi->ty = (m->b * m->tx - m->a * m->ty)/det;
}


