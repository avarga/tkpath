/*
 * tkCanvPathUtil.h --
 *
 *	    This file implements style objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007  Mats Bengtsson
 *
 * $Id$
 */

#ifndef INCLUDED_TKCANVPATHUTIL_H
#define INCLUDED_TKCANVPATHUTIL_H

#include "tkIntPath.h"

#ifdef __cplusplus
extern "C" {
#endif

int			Tk_ConfigPathStylesGC(Tk_Canvas canvas, Tk_Item *itemPtr, 
                    Tk_PathStyle *stylePtr);
int 		Tk_ConfigStrokePathStyleGC(XGCValues *gcValues, Tk_Canvas canvas,
                    Tk_Item *item, Tk_PathStyle *style);
int 		Tk_ConfigFillPathStyleGC(XGCValues *gcValues, Tk_Canvas canvas,
                    Tk_Item *item, Tk_PathStyle *style);

int			CoordsForPointItems(Tcl_Interp *interp, Tk_Canvas canvas, 
                    double *pointPtr, int objc, Tcl_Obj *CONST objv[]);
int			CoordsForRectangularItems(Tcl_Interp *interp, Tk_Canvas canvas, 
                    PathRect *rectPtr, int objc, Tcl_Obj *CONST objv[]);
PathRect	GetGenericBarePathBbox(PathAtom *atomPtr);
PathRect	GetGenericPathTotalBboxFromBare(PathAtom *atomPtr, Tk_PathStyle *stylePtr, PathRect *bboxPtr);
void		SetGenericPathHeaderBbox(Tk_Item *headerPtr, TMatrix *mPtr,
                    PathRect *totalBboxPtr);
TMatrix		GetCanvasTMatrix(Tk_Canvas canvas);
PathRect	NewEmptyPathRect(void);
int			IsPathRectEmpty(PathRect *r);
void		IncludePointInRect(PathRect *r, double x, double y);
double		GenericPathToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, Tk_PathStyle *stylePtr,
                    PathAtom *atomPtr, int maxNumSegments, double *pointPtr);
int			GenericPathToArea(Tk_Canvas canvas,	Tk_Item *itemPtr, Tk_PathStyle *stylePtr,
                    PathAtom * atomPtr, int maxNumSegments, double *areaPtr);
void		TranslatePathAtoms(PathAtom *atomPtr, double deltaX, double deltaY);
void		ScalePathAtoms(PathAtom *atomPtr, double originX, double originY,
                    double scaleX, double scaleY);
void		TranslatePathRect(PathRect *r, double deltaX, double deltaY);


/* For processing custom options. */

int		FillRuleParseProc(ClientData clientData,
                     Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
char *	FillRulePrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		TextAnchorParseProc(ClientData clientData,
                     Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
char *	TextAnchorPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		GradientParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
char *	GradientPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		MatrixParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
char *	MatrixPrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);
int		StyleParseProc(ClientData clientData,
                        Tcl_Interp *interp, Tk_Window tkwin,
                        CONST char *value, char *recordPtr, int offset);
char *	StylePrintProc(ClientData clientData, Tk_Window tkwin, 
                        char *widgRec, int offset, Tcl_FreeProc **freeProcPtr);

/*
 * The canvas 'Area' and 'Point' functions.
 */
int			PathPolyLineToArea(double *polyPtr, int numPoints, register double *rectPtr);
double		PathThickPolygonToPoint(int joinStyle, int capStyle, double width, 
                    int isclosed, double *polyPtr, int numPoints, double *pointPtr);
double		PathPolygonToPointEx(double *polyPtr, int numPoints, double *pointPtr, 
                    int *intersectionsPtr, int *nonzerorulePtr);
double		PathRectToPoint(double rectPtr[], double width, int filled, double pointPtr[]);
int			PathRectToArea(double rectPtr[], double width, int filled, double *areaPtr);
int			PathRectToAreaWithMatrix(PathRect bbox, TMatrix *mPtr, double *areaPtr);
int			PathRectToPointWithMatrix(PathRect bbox, TMatrix *mPtr, double *pointPtr);


/*
 * Information used for parsing configuration specs.  If you change any
 * of the default strings, be sure to change the corresponding default
 * values in CreatePath.
 */

#define PATH_STYLE_CUSTOM_OPTION_STATE   \
    static Tk_CustomOption stateOption = {                \
        (Tk_OptionParseProc *) PathTkStateParseProc,      \
        PathTkStatePrintProc,                             \
        (ClientData) 2                                    \
    };
    
#define PATH_STYLE_CUSTOM_OPTION_TAGS   \
    static Tk_CustomOption tagsOption = {                 \
        (Tk_OptionParseProc *) PathTk_CanvasTagsParseProc,\
        PathTk_CanvasTagsPrintProc,                       \
        (ClientData) NULL                                 \
    };
    
#define PATH_STYLE_CUSTOM_OPTION_DASH   \
    static Tk_CustomOption dashOption = {                 \
        (Tk_OptionParseProc *) PathTkCanvasDashParseProc, \
        PathTkCanvasDashPrintProc,                        \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_PIXEL   \
    static Tk_CustomOption pixelOption = {                \
        (Tk_OptionParseProc *) PathTkPixelParseProc,      \
        PathTkPixelPrintProc,                             \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_FILLRULE   \
    static Tk_CustomOption fillRuleOption = {             \
        (Tk_OptionParseProc *) FillRuleParseProc,         \
        FillRulePrintProc,                                \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_GRADIENT   \
    static Tk_CustomOption gradientOption = {             \
        (Tk_OptionParseProc *) GradientParseProc,   	  \
        GradientPrintProc,                          	  \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_MATRIX   \
    static Tk_CustomOption matrixOption = {               \
        (Tk_OptionParseProc *) MatrixParseProc,           \
        MatrixPrintProc,                                  \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_STYLE   \
    static Tk_CustomOption styleOption = {                \
        (Tk_OptionParseProc *) StyleParseProc,            \
        StylePrintProc,                                   \
        (ClientData) NULL                                 \
    };

#define PATH_STYLE_CUSTOM_OPTION_RECORDS   \
    PATH_STYLE_CUSTOM_OPTION_STATE                        \
    PATH_STYLE_CUSTOM_OPTION_TAGS                         \
    PATH_STYLE_CUSTOM_OPTION_DASH                         \
    PATH_STYLE_CUSTOM_OPTION_PIXEL                        \
    PATH_STYLE_CUSTOM_OPTION_FILLRULE                     \
    PATH_STYLE_CUSTOM_OPTION_GRADIENT                     \
    PATH_STYLE_CUSTOM_OPTION_MATRIX                       \
    PATH_STYLE_CUSTOM_OPTION_STYLE


#define PATH_CONFIG_SPEC_STYLE_FILL(typeName, theColor)                     \
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,                \
        theColor, Tk_Offset(typeName, style.fillColor), TK_CONFIG_NULL_OK}, \
    {TK_CONFIG_CUSTOM, "-fillgradient", (char *) NULL, (char *) NULL,       \
        (char *) NULL, Tk_Offset(typeName, style.gradientFillName),         \
        TK_CONFIG_NULL_OK, &gradientOption},                                \
    {TK_CONFIG_DOUBLE, "-fillopacity", (char *) NULL, (char *) NULL,        \
        "1.0", Tk_Offset(typeName, style.fillOpacity), 0},                  \
    {TK_CONFIG_CUSTOM, "-fillrule", (char *) NULL, (char *) NULL,           \
        "nonzero", Tk_Offset(typeName, style.fillRule),                     \
        TK_CONFIG_DONT_SET_DEFAULT, &fillRuleOption}
        
#define PATH_CONFIG_SPEC_STYLE_MATRIX(typeName)                             \
    {TK_CONFIG_CUSTOM, "-matrix", (char *) NULL, (char *) NULL,             \
        (char *) NULL, Tk_Offset(typeName, style.matrixPtr),                \
        TK_CONFIG_NULL_OK, &matrixOption}

#define PATH_CONFIG_SPEC_STYLE_STROKE(typeName, theColor)                   \
    {TK_CONFIG_COLOR, "-stroke", (char *) NULL, (char *) NULL,              \
        theColor, Tk_Offset(typeName, style.strokeColor),                   \
        TK_CONFIG_NULL_OK},                                                 \
    {TK_CONFIG_CUSTOM, "-strokedasharray", (char *) NULL, (char *) NULL,    \
        (char *) NULL, Tk_Offset(typeName, style.dash),                     \
        TK_CONFIG_NULL_OK, &dashOption},                                    \
        \
    /* @@@ TODO */   \
    {TK_CONFIG_CUSTOM, "-strokegradient", (char *) NULL, (char *) NULL,     \
        (char *) NULL, Tk_Offset(typeName, style.gradientStrokeName),       \
        TK_CONFIG_NULL_OK, &gradientOption},                                \
    \
    {TK_CONFIG_CAP_STYLE, "-strokelinecap", (char *) NULL, (char *) NULL,   \
        "butt", Tk_Offset(typeName, style.capStyle),                        \
        TK_CONFIG_DONT_SET_DEFAULT},                                        \
    {TK_CONFIG_JOIN_STYLE, "-strokelinejoin", (char *) NULL, (char *) NULL, \
        "round", Tk_Offset(typeName, style.joinStyle),                      \
        TK_CONFIG_DONT_SET_DEFAULT},                                        \
    {TK_CONFIG_DOUBLE, "-strokemiterlimit", (char *) NULL, (char *) NULL,   \
        "4.0", Tk_Offset(typeName, style.miterLimit), 0},                   \
    {TK_CONFIG_DOUBLE, "-strokeopacity", (char *) NULL, (char *) NULL,      \
        "1.0", Tk_Offset(typeName, style.strokeOpacity), 0},                \
    {TK_CONFIG_BITMAP, "-strokestipple", (char *) NULL, (char *) NULL,      \
        (char *) NULL, Tk_Offset(typeName, style.strokeStipple),            \
        TK_CONFIG_NULL_OK},                                                 \
    {TK_CONFIG_CUSTOM, "-strokewidth", (char *) NULL, (char *) NULL,        \
        "1.0", Tk_Offset(typeName, style.strokeWidth),                      \
        TK_CONFIG_DONT_SET_DEFAULT, &pixelOption}

/*
 * This macro REQUIRES that we have a 'styleName' struct member.
 */

#define PATH_CONFIG_SPEC_CORE(typeName)  \
    {TK_CONFIG_CUSTOM, "-state", (char *) NULL, (char *) NULL,              \
        (char *) NULL, Tk_Offset(Tk_Item, state), TK_CONFIG_NULL_OK,        \
        &stateOption},                                                      \
    {TK_CONFIG_CUSTOM, "-style", (char *) NULL, (char *) NULL,              \
        (char *) NULL, Tk_Offset(typeName, styleName),                      \
        TK_CONFIG_DONT_SET_DEFAULT, &styleOption},                          \
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,               \
        (char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption}


#define PATH_END_CONFIG_SPEC   \
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,            \
        (char *) NULL, 0, 0}


#ifdef __cplusplus
}
#endif

#endif      // INCLUDED_TKCANVPATHUTIL_H

