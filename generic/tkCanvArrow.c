/*
 * tkCanvArrow.c --
 *
 *	This file implements arrow heads
 *
 * Copyright (c) 2014 OpenSim Ltd., author:Zoltan Bojthe
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkpCanvas.h"
#include "tkCanvArrow.h"
#include "tkCanvPathUtil.h"
#include "tkPathStyle.h"

#ifdef _MSC_VER
#define isnan(x) ((x) != (x))
#endif

static const double zero = 0.0; // just for NaN
#define NaN (zero/zero)

PathAtom *
MakePathAtomsFromArrow(ArrowDescr *arrowDescr)
{
    PathPoint *coords = arrowDescr->arrowPointsPtr;
    PathAtom *atomPtr = NULL, *ret = NULL;
    if (coords)
    {
        int i = 0;
        if (isnan(coords[0].x) || isnan(coords[0].y)) {
            i = 1;
        }
        ret = atomPtr = NewMoveToAtom(coords[i].x, coords[i].y);
        for (i++ ; i < DRAWABLE_PTS_IN_ARROW; i++) {
            if (isnan(coords[i].x) || isnan(coords[i].y))
                continue;
            atomPtr->nextPtr = NewLineToAtom(coords[i].x, coords[i].y);
            atomPtr = atomPtr->nextPtr;
        }
    }
    return ret;
}

void
DisplayArrow(Tk_PathCanvas canvas, Drawable drawable, ArrowDescr *arrowDescr,
        Tk_PathStyle *const style, TMatrix *mPtr, PathRect *bboxPtr)
{
    if (arrowDescr->arrowEnabled && arrowDescr->arrowPointsPtr != NULL) {
        Tk_PathStyle arrowStyle = *style;
        TkPathColor fc;
        PathAtom *atomPtr;

        if (arrowDescr->arrowFillRatio > 0.0 && arrowDescr->arrowLength != 0.0) {
            arrowStyle.strokeWidth = 0.0;
            fc.color = arrowStyle.strokeColor;
            fc.gradientInstPtr = NULL;
            arrowStyle.fill = &fc;
            arrowStyle.fillOpacity = arrowStyle.strokeOpacity;
        } else {
            arrowStyle.fill = NULL;
            arrowStyle.fillOpacity = 1.0;
            arrowStyle.joinStyle = 1;
        }
        atomPtr = MakePathAtomsFromArrow(arrowDescr);
        TkPathDrawPath(Tk_PathCanvasTkwin(canvas), drawable, atomPtr, &arrowStyle, mPtr, bboxPtr);
        TkPathFreeAtoms(atomPtr);
    }
}

void TkPathArrowDescrInit(ArrowDescr *descrPtr)
{
    descrPtr->arrowEnabled = ARROWS_OFF;
    descrPtr->arrowLength = (float)8.0;
    descrPtr->arrowWidth = (float)4.0;
    descrPtr->arrowFillRatio = (float)1.0;
    descrPtr->arrowPointsPtr = NULL;
}

void IncludeArrowPointsInRect(PathRect *bbox, ArrowDescr *arrowDescrPtr)
{
    if (arrowDescrPtr->arrowEnabled && arrowDescrPtr->arrowPointsPtr) {
        int i;
        for (i = 0; i < PTS_IN_ARROW; i++)
            if (!isnan(arrowDescrPtr->arrowPointsPtr[i].x) && ! isnan(arrowDescrPtr->arrowPointsPtr[i].y))
                IncludePointInRect(bbox, arrowDescrPtr->arrowPointsPtr[i].x, arrowDescrPtr->arrowPointsPtr[i].y);
    }
}

void TkPathIncludeArrowPoints(Tk_PathItem *itemPtr, ArrowDescr *arrowDescrPtr)
{
    if (arrowDescrPtr->arrowEnabled) {
        int i;
        for (i = 0; i < PTS_IN_ARROW; i++)
            if (!isnan(arrowDescrPtr->arrowPointsPtr[i].x) && ! isnan(arrowDescrPtr->arrowPointsPtr[i].y))
                TkPathIncludePoint(itemPtr, (double *)&arrowDescrPtr->arrowPointsPtr[i]);
    }
}

void TkPathPreconfigureArrow(PathPoint *pf, ArrowDescr *arrowDescr)
{
    if (arrowDescr->arrowPointsPtr == NULL) {
        if (arrowDescr->arrowEnabled) {
            arrowDescr->arrowPointsPtr = (PathPoint *)ckalloc((unsigned)(PTS_IN_ARROW * sizeof(PathPoint)));
            arrowDescr->arrowPointsPtr[LINE_PT_IN_ARROW] = *pf;
            arrowDescr->arrowPointsPtr[ORIG_PT_IN_ARROW] = *pf;
        }
    } else {
        if (pf->x == arrowDescr->arrowPointsPtr[LINE_PT_IN_ARROW].x
                && pf->y == arrowDescr->arrowPointsPtr[LINE_PT_IN_ARROW].y) {
            *pf = arrowDescr->arrowPointsPtr[ORIG_PT_IN_ARROW];
        }
        if (!arrowDescr->arrowEnabled) {
            ckfree((char *)arrowDescr->arrowPointsPtr);
            arrowDescr->arrowPointsPtr = NULL;
        }
    }
}

PathPoint TkPathConfigureArrow(PathPoint pf, PathPoint pl, ArrowDescr *arrowDescr,
        Tk_PathStyle *lineStyle, int updateFirstPoint)
{
    if (arrowDescr->arrowEnabled) {
        PathPoint p0;
        double lineWidth = lineStyle->strokeWidth;
        double shapeLength = arrowDescr->arrowLength;
        double shapeWidth = arrowDescr->arrowWidth;
        double shapeFill = arrowDescr->arrowFillRatio;
        double dx, dy, length, sinTheta, cosTheta;
        double backup;          /* Distance to backup end points so the line
                                 * ends in the middle of the arrowhead. */
        double minsShapeFill;
        PathPoint *poly = arrowDescr->arrowPointsPtr;
        int capStyle = lineStyle->capStyle;    /*  CapButt, CapProjecting, or CapRound. */

        if (!poly) {
            Tcl_Panic("Internal error: PathPoint list is NULL pointer\n");
        }
        if (shapeWidth < lineWidth) {
            shapeWidth = lineWidth;
        }
        minsShapeFill = lineWidth*shapeLength/shapeWidth;
        if (shapeFill > 0.0 && fabs(shapeLength*shapeFill) < fabs(minsShapeFill))
            shapeFill = 1.1*minsShapeFill / shapeLength;

        backup = 0.0;
        if (lineWidth > 1.0) {
            backup = (capStyle == CapProjecting) ? 0.5 * lineWidth : 0.0;
            if (shapeFill > 0.0 && shapeLength != 0.0) {
                backup += 0.5 * lineWidth * shapeLength / shapeWidth;
            }
        }

        dx = pf.x - pl.x;
        dy = pf.y - pl.y;
        length = hypot(dx, dy);
        if (length == 0) {
            sinTheta = cosTheta = 0.0;
        } else {
            sinTheta = dy/length;
            cosTheta = dx/length;
        }

        p0.x = pf.x - shapeLength * cosTheta;
        p0.y = pf.y - shapeLength * sinTheta;
        if (shapeFill > 0.0 && shapeLength != 0.0) {
            poly[0].x = pf.x - shapeLength * shapeFill * cosTheta;
            poly[0].y = pf.y - shapeLength * shapeFill * sinTheta;
            poly[4] = poly[0];
        } else {
            poly[0].x = poly[0].y = poly[4].x = poly[4].y = NaN;
        }
        poly[1].x = p0.x - shapeWidth * sinTheta;
        poly[1].y = p0.y + shapeWidth * cosTheta;
        poly[2].x = pf.x;
        poly[2].y = pf.y;
        poly[3].x = p0.x + shapeWidth * sinTheta;
        poly[3].y = p0.y - shapeWidth * cosTheta;
        /*
         * Polygon done. Now move the first point towards the second so that
         * the corners at the end of the line are inside the arrowhead.
         */

        poly[LINE_PT_IN_ARROW] = poly[ORIG_PT_IN_ARROW];
        if (updateFirstPoint) {
            poly[LINE_PT_IN_ARROW].x -= backup*cosTheta;
            poly[LINE_PT_IN_ARROW].y -= backup*sinTheta;
        }

        return poly[LINE_PT_IN_ARROW];
    }
    return pf;
}

void TkPathTranslateArrow(ArrowDescr *arrowDescr, double deltaX, double deltaY)
{
    if (arrowDescr->arrowPointsPtr != NULL) {
        int i;
        for (i = 0; i < PTS_IN_ARROW; i++) {
            arrowDescr->arrowPointsPtr[i].x += deltaX;
            arrowDescr->arrowPointsPtr[i].y += deltaY;
        }
    }
}

void TkPathScaleArrow(ArrowDescr *arrowDescr, double originX, double originY, double scaleX, double scaleY)
{
    if (arrowDescr->arrowPointsPtr != NULL) {
        int i;
        PathPoint *pt;
        for (i = 0, pt = arrowDescr->arrowPointsPtr; i < PTS_IN_ARROW; i++, pt++) {
            pt->x = originX + scaleX*(pt->x - originX);
            pt->y = originX + scaleX*(pt->y - originX);
        }
    }
}

void TkPathFreeArrow(ArrowDescr *arrowDescr)
{
    if (arrowDescr->arrowPointsPtr != NULL) {
        ckfree((char *)arrowDescr->arrowPointsPtr);
        arrowDescr->arrowPointsPtr = NULL;
    }
}

typedef PathPoint *PathPointPtr;

int
getSegmentsFromPathAtomList(PathAtom *firstAtom,
        PathPoint **firstPt, PathPoint *secondPt,
        PathPoint *penultPt, PathPoint **lastPt)
{
    PathAtom *atom;
    int i;

    *firstPt = *lastPt = NULL;
    secondPt->x = secondPt->y = penultPt->x = penultPt->y = NaN;

    if (firstAtom && firstAtom->type != PATH_ATOM_M) {
        Tcl_Panic("Invalid path! Path must start with M(move) atom");
    }
    for (i = 0, atom = firstAtom; atom; atom = atom->nextPtr) {
        switch (atom->type) {
            case PATH_ATOM_M:
            {
                MoveToAtom *moveto = (MoveToAtom *)atom;
                if (i == 0) {
                    *firstPt = (PathPointPtr)&moveto->x;
                    i++;
                } else if (i == 1) {
                    secondPt->x = moveto->x;
                    secondPt->y = moveto->y;
                    i++;
                }
                penultPt->x = penultPt->y = NaN;
                *lastPt = (PathPointPtr)&moveto->x;
                break;
            }
            case PATH_ATOM_L:
            {
                LineToAtom *lineto = (LineToAtom *)atom;
                if (i == 1) {
                    secondPt->x = lineto->x;
                    secondPt->y = lineto->y;
                    i++;
                }
                *penultPt = **lastPt;
                *lastPt = (PathPointPtr)&lineto->x;
                break;
            }
            case PATH_ATOM_A: {
                ArcAtom *arc = (ArcAtom *) atom;

                /*
                      Draw an elliptical arc from the current point to (x, y).
                  The points are on an ellipse with x-radius <radX> and y-radius <radY>.
                  The ellipse is rotated by <angle> degrees. If the arc is less than
                  180 degrees, <largeArcFlag> is zero, else it is one. If the arc is to be
                  drawn in cw direction, sweepFlag is one, and zero for the ccw
                  direction.
                  NB: the start and end points may not coincide else the result
                  is undefined. If you want to make a circle just do two 180 degree arcs.
                 */
                int result;
                double cx, cy, rx, ry;
                double theta1, dtheta;
                PathPoint startPt = **lastPt;
                double phi = DEGREES_TO_RADIANS * arc->angle;

                result = EndpointToCentralArcParameters(
                        startPt.x, startPt.y,
                        arc->x, arc->y, arc->radX, arc->radY,
                        phi,
                        arc->largeArcFlag, arc->sweepFlag,
                        &cx, &cy, &rx, &ry,
                        &theta1, &dtheta);
                if (result == kPathArcOK) {
                    double sinTheta2, cosTheta2;
                    double sinPhi = sin(phi);
                    double cosPhi = cos(phi);
                    double theta2 = theta1 + dtheta;

                    if (dtheta > 0.0) {
                        theta1 += M_PI*0.01;
                        theta2 -= M_PI*0.01;
                    } else {
                        theta1 -= M_PI*0.01;
                        theta2 += M_PI*0.01;
                    }

                    sinTheta2 = sin(theta2);
                    cosTheta2 = cos(theta2);

                    if (i == 1) {
                        double sinTheta1 = sin(theta1);
                        double cosTheta1 = cos(theta1);
                        /* auxiliary point 1 */
                        secondPt->x = cx + rx * cosTheta1 * cosPhi - ry * sinTheta1 * sinPhi;
                        secondPt->y = cy + rx * cosTheta1 * sinPhi + ry * sinTheta1 * cosPhi;
                        i++;
                    }
                    /* auxiliary point 2 */
                    penultPt->x = cx + rx * cosTheta2 * cosPhi - ry * sinTheta2 * sinPhi;
                    penultPt->y = cy + rx * cosTheta2 * sinPhi + ry * sinTheta2 * cosPhi;
                } else {
                    /* arc is line */
                    if (i == 1) {
                        secondPt->x = arc->x;
                        secondPt->y = arc->y;
                        i++;
                    }
                    *penultPt = **lastPt;
                }

                *lastPt = (PathPointPtr)&arc->x;
                break;
            }
            case PATH_ATOM_Q: {
                QuadBezierAtom *quad = (QuadBezierAtom *) atom;
                if (i == 1) {
                    secondPt->x = quad->ctrlX;
                    secondPt->y = quad->ctrlY;
                    i++;
                }
                penultPt->x = quad->ctrlX;
                penultPt->y = quad->ctrlY;
                *lastPt = (PathPointPtr)&quad->anchorX;
                break;
            }
            case PATH_ATOM_C: {
                CurveToAtom *curve = (CurveToAtom *) atom;
                if (i == 1) {
                    secondPt->x = curve->ctrlX1;
                    secondPt->y = curve->ctrlY1;
                    i++;
                }
                penultPt->x = curve->ctrlX2;
                penultPt->y = curve->ctrlY2;
                *lastPt = (PathPointPtr)&curve->anchorX;
                break;
            }
            case PATH_ATOM_Z: {
                CloseAtom *closeAtom = (CloseAtom *) atom;
                *penultPt = **lastPt;
                *lastPt = (PathPointPtr)&closeAtom->x;
                break;
            }
            case PATH_ATOM_ELLIPSE:
            case PATH_ATOM_RECT: {
                /* Empty. */
                break;
            }
            default:
                break;
        }
    }
    return (i >= 2) ? TCL_OK : TCL_ERROR;
}

/*----------------------------------------------------------------------*/

