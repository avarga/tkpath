/*
 * tkPathUtil.h --
 *
 *		This file contains support functions for tkpath.
 *
 * Copyright (c) 2005  Mats Bengtsson
 *
 * $Id$
 */

#include <tkInt.h>
#include "tkPath.h"
#include "tkIntPath.h"



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

/*
 * Miscellaneous matrix utilities.
 */
 
void 
PathApplyTMatrix(TMatrix *m, double *x, double *y)
{
    if (m == NULL) {
        return;
    }
    double tmpx = *x;
    double tmpy = *y;
    *x = tmpx*m->a + tmpy*m->c + m->tx;
    *y = tmpx*m->b + tmpy*m->d + m->ty;
}

void 
PathApplyTMatrixToPoint(TMatrix *m, double in[2], double out[2])
{
    if (m == NULL) {
        return;
    }
    *out     = in[0]*m->a + in[1]*m->c + m->tx;
    *(out+1) = in[0]*m->b + in[1]*m->d + m->ty;
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

/*
 *----------------------------------------------------------------------
 *
 * PathGetTMatrix --
 *
 *		Parses a Tcl list (in string) into a TMatrix record.  
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
PathGetTMatrix(
        Tcl_Interp* interp, 
        char *list, 			/* Object containg the lists for the matrix. */
        TMatrix *matrixPtr)		/* Where to store TMatrix corresponding
                                 * to list. Must be allocated! */
{
    char **argv = NULL, **rowArgv = NULL;
    int i, j, argc, rowArgc;
    double tmp[3][2];
    int result = TCL_OK;

    /* Check matrix consistency. */
    if (Tcl_SplitList(interp, list, &argc, &argv) != TCL_OK) {
        result = TCL_ERROR;
        goto bail;
    }
    if (argc != 3) {
        Tcl_AppendResult(interp, "matrix \"", list, "\" is inconsistent",
                (char *) NULL);
        result = TCL_ERROR;
        goto bail;
    }
    
    /* Take each row in turn. */
    for (i = 0; i < 3; i++) {
        if (Tcl_SplitList(interp, argv[i], &rowArgc, &rowArgv) != TCL_OK) {
            result = TCL_ERROR;
            goto bail;
        }
        if (rowArgc != 2) {
            Tcl_AppendResult(interp, "matrix \"", list, "\" is inconsistent",
                    (char *) NULL);
            result = TCL_ERROR;
            goto bail;
        }
        for (j = 0; j < 2; j++) {
            if (Tcl_GetDouble(interp, rowArgv[j], &(tmp[i][j])) != TCL_OK) {
                Tcl_AppendResult(interp, "matrix \"", list, "\" is inconsistent",
                        (char *) NULL);
                result = TCL_ERROR;
                goto bail;
            }
        }
        if (rowArgv != NULL) {
            Tcl_Free((char *) rowArgv);
            rowArgv = NULL;
        }
    }
        
    /* Check that the matrix is not close to being singular. */
    if (fabs(tmp[0][0]*tmp[1][1] - tmp[0][1]*tmp[1][0]) < 1e-6) {
        Tcl_AppendResult(interp, "matrix \"", list, "\" is close to singular",
                (char *) NULL);
            result = TCL_ERROR;
            goto bail;
    }
        
    /* Matrix. */
    matrixPtr->a  = tmp[0][0];
    matrixPtr->b  = tmp[0][1];
    matrixPtr->c  = tmp[1][0];
    matrixPtr->d  = tmp[1][1];
    matrixPtr->tx = tmp[2][0];
    matrixPtr->ty = tmp[2][1];
    
bail:
    if (argv != NULL) {
        Tcl_Free((char *) argv);
    }
    if (rowArgv != NULL) {
        Tcl_Free((char *) rowArgv);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PathGetTclObjFromTMatrix --
 *
 *		Parses a TMatrix record into a list object.
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
PathGetTclObjFromTMatrix(
        Tcl_Interp* interp, 
        TMatrix *matrixPtr,
        Tcl_Obj **listObjPtrPtr)
{
	Tcl_Obj		*listObj, *subListObj;
    
    /* @@@ Error handling remains. */

    listObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    if (matrixPtr != NULL) {
        subListObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->a));
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->b));
        Tcl_ListObjAppendElement(interp, listObj, subListObj);
        
        subListObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->c));
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->d));
        Tcl_ListObjAppendElement(interp, listObj, subListObj);
        
        subListObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->tx));
        Tcl_ListObjAppendElement(interp, subListObj, Tcl_NewDoubleObj(matrixPtr->ty));
        Tcl_ListObjAppendElement(interp, listObj, subListObj);
    }
    *listObjPtrPtr = listObj;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PathGenericCmdDispatcher --
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
PathGenericCmdDispatcher( 
        Tcl_Interp* interp,
        int objc,
      	Tcl_Obj* CONST objv[],
        char *baseName,
        int *baseNameUIDPtr,
        Tcl_HashTable *hashTablePtr,
        Tk_OptionTable optionTable,
        char *(*createAndConfigProc)(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]),
        void (*configNotifyProc)(char *recordPtr, int mask, int objc, Tcl_Obj *CONST objv[]),
        void (*freeProc)(Tcl_Interp *interp, char *recordPtr))
{
    char   		*name;
    char 		*recordPtr;
    int 		result = TCL_OK;
    int 		index;
    int			mask;
    Tcl_HashEntry *hPtr;
    Tk_Window tkwin = Tk_MainWindow(interp); /* Should have been the canvas. */

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
			resultObjPtr = Tk_GetOptionValue(interp, recordPtr, optionTable, objv[3], tkwin);
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
                        tkwin);
				if (resultObjPtr == NULL) {
					return TCL_ERROR;
                }
				Tcl_SetObjResult(interp, resultObjPtr);
			} else {
				if (Tk_SetOptions(interp, recordPtr, optionTable, objc - 3, objv + 3, 
                        tkwin, NULL, &mask) != TCL_OK) {
					return TCL_ERROR;
                }
                if (configNotifyProc != NULL) {
                    (*configNotifyProc)(recordPtr, mask, objc - 3, objv + 3);
                }
			}
            break;
        }
        
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
            (*baseNameUIDPtr)++;
			recordPtr = (*createAndConfigProc)(interp, str, objc - 2, objv + 2);
			if (recordPtr == NULL) {
				return TCL_ERROR;
            }
            
            if (Tk_InitOptions(interp, recordPtr, optionTable, 
                    tkwin) != TCL_OK) {
                ckfree(recordPtr);
                return TCL_ERROR;
            }
            if (Tk_SetOptions(interp, recordPtr, optionTable, 	
                    objc - 2, objv + 2, tkwin, NULL, &mask) != TCL_OK) {
                Tk_FreeConfigOptions(recordPtr, optionTable, NULL);
                ckfree(recordPtr);
                return TCL_ERROR;
            }
            if (configNotifyProc != NULL) {
                (*configNotifyProc)(recordPtr, mask, objc - 2, objv + 2);
            }

			hPtr = Tcl_CreateHashEntry(hashTablePtr, str, &isNew);
			Tcl_SetHashValue(hPtr, recordPtr);
			Tcl_SetObjResult(interp, Tcl_NewStringObj(str, -1));
            break;
        }
                
        case kPathGenericCmdDelete: {
			if (objc < 3) {
				Tcl_WrongNumArgs(interp, 2, objv, "name");
				return TCL_ERROR;
			}
            name = Tcl_GetString(objv[2]);
            hPtr = Tcl_FindHashEntry(hashTablePtr, name);
            recordPtr = Tcl_GetHashValue(hPtr);
			if (hPtr != NULL) {
                Tcl_DeleteHashEntry(hPtr);
			}
            (*freeProc)(interp, recordPtr);
			break;
        }
        
        case kPathGenericCmdNames: {
			Tcl_Obj *listObj;
			Tcl_HashSearch search;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(hashTablePtr, &search);
			while (hPtr != NULL) {
                name = Tcl_GetHashKey(hashTablePtr, hPtr);
				Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(name, -1));
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
 * ObjectIsEmpty --
 *
 *	This procedure tests whether the string value of an object is
 *	empty.
 *
 * Results:
 *	The return value is 1 if the string value of objPtr has length
 *	zero, and 0 otherwise.
 *
 * Side effects:
 *	May cause object shimmering, since this function can force a
 *	conversion to a string object.
 *
 *----------------------------------------------------------------------
 */

int
ObjectIsEmpty(
        Tcl_Obj *objPtr)	/* Object to test.  May be NULL. */
{
    int length;

    if (objPtr == NULL) {
        return 1;
    }
    if (objPtr->bytes != NULL) {
        return (objPtr->length == 0);
    }
    Tcl_GetStringFromObj(objPtr, &length);
    return (length == 0);
}

/*-------------------------------------------------------------------------*/

