/*
 * tkPathStyle.h --
 *
 *	    This file contains definitions for style objects used when drawing paths.
 *		Mostly used for option parsing.
 *
 * Copyright (c) 2007  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"


int 		MatrixSetOption(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                    Tcl_Obj **value, char *recordPtr, int internalOffset, char *oldInternalPtr, int flags);
Tcl_Obj *	MatrixGetOption(ClientData clientData, Tk_Window tkwin, char *recordPtr, int internalOffset);
void		MatrixRestoreOption(ClientData clientData, Tk_Window tkwin, char *internalPtr, char *oldInternalPtr);
void		MatrixFreeOption(ClientData clientData, Tk_Window tkwin, char *internalPtr);
int 		DashSetOption(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                    Tcl_Obj **value, char *recordPtr, int internalOffset, char *oldInternalPtr, int flags);
Tcl_Obj *	DashGetOption(ClientData clientData, Tk_Window tkwin, char *recordPtr, int internalOffset);
void		DashFreeOption(ClientData clientData, Tk_Window tkwin, char *internalPtr);
int 		PathColorSetOption(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                    Tcl_Obj **value, char *recordPtr, int internalOffset, char *oldInternalPtr, int flags);
Tcl_Obj *	PathColorGetOption(ClientData clientData, Tk_Window tkwin, char *recordPtr, int internalOffset);
void		PathColorRestoreOption(ClientData clientData, Tk_Window tkwin, char *internalPtr, char *oldInternalPtr);
void		PathColorFreeOption(ClientData clientData, Tk_Window tkwin, char *internalPtr);


#define PATH_STYLE_CUSTOM_OPTION_MATRIX			\
    static Tk_ObjCustomOption matrixCO = {		\
        "matrix",								\
        MatrixSetOption,						\
        MatrixGetOption,						\
        MatrixRestoreOption,					\
        MatrixFreeOption,						\
        (ClientData) NULL						\
    };

#define PATH_STYLE_CUSTOM_OPTION_DASH			\
    static Tk_ObjCustomOption dashCO = {		\
        "dash",									\
        DashSetOption,							\
        DashGetOption,							\
        NULL,									\
        DashFreeOption,							\
        (ClientData) NULL						\
    };
    
#define PATH_STYLE_CUSTOM_OPTION_PATHCOLOR
    static Tk_ObjCustomOption pathColorCO = {\
        "pathcolor",							\
        PathColorSetOption,						\
        PathColorGetOption,						\
        PathColorRestoreOption,					\
        PathColorFreeOption,					\
        (ClientData) NULL						\
    };


#define PATH_STYLE_CUSTOM_OPTION_RECORDS	 	\
    PATH_STYLE_CUSTOM_OPTION_MATRIX 			\
    PATH_STYLE_CUSTOM_OPTION_DASH				\
    PATH_STYLE_CUSTOM_OPTION_PATHCOLOR

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

/*
 * This assumes that we have a Tk_PathStyle struct element named 'style'.
 */

#define PATH_OPTION_SPEC_STYLE_FILL(typeName, theColor)                     \
	{TK_OPTION_CUSTOM, "-fill", (char *) NULL, (char *) NULL,				\
		theColor, -1, Tk_Offset(typeName, style.fill),				\
		TK_OPTION_NULL_OK, (ClientData) &pathColorCO,	 					\
        PATH_STYLE_OPTION_FILL},											\
    {TK_OPTION_DOUBLE, "-fillopacity", (char *) NULL, (char *) NULL,        \
        "1.0", -1, Tk_Offset(typeName, style.fillOpacity), 0, 0,            \
        PATH_STYLE_OPTION_FILL_OPACITY},                                    \
    {TK_OPTION_STRING_TABLE, "-fillrule", (char *) NULL, (char *) NULL,     \
        "nonzero", -1, Tk_Offset(typeName, style.fillRule),                 \
        0, (ClientData) fillRuleST, PATH_STYLE_OPTION_FILL_RULE}

#define PATH_OPTION_SPEC_STYLE_MATRIX(typeName)                             \
	{TK_OPTION_CUSTOM, "-matrix", (char *) NULL, (char *) NULL,             \
		(char *) NULL, -1, Tk_Offset(typeName, style.matrixPtr),            \
		TK_OPTION_NULL_OK, (ClientData) &matrixCO, PATH_STYLE_OPTION_MATRIX}

#define PATH_OPTION_SPEC_STYLE_STROKE(typeName, theColor)                   \
    {TK_OPTION_COLOR, "-stroke", (char *) NULL, (char *) NULL,              \
        theColor, -1, Tk_Offset(typeName, style.strokeColor),               \
        TK_OPTION_NULL_OK, 0, PATH_STYLE_OPTION_STROKE},                    \
	{TK_OPTION_CUSTOM, "-strokedasharray", (char *) NULL, (char *) NULL,    \
		(char *) NULL, -1, Tk_Offset(typeName, style.dash),                 \
		TK_OPTION_NULL_OK, (ClientData) &dashCO,                            \
        PATH_STYLE_OPTION_STROKE_DASHARRAY},                                \
    {TK_OPTION_STRING_TABLE, "-strokelinecap", (char *) NULL, (char *) NULL,\
        "butt", -1, Tk_Offset(typeName, style.capStyle),                    \
        0, (ClientData) lineCapST, PATH_STYLE_OPTION_STROKE_LINECAP},       \
    {TK_OPTION_STRING_TABLE, "-strokelinejoin", (char *) NULL, (char *) NULL,\
        "round", -1, Tk_Offset(typeName, style.joinStyle),                  \
        0, (ClientData) lineJoinST, PATH_STYLE_OPTION_STROKE_LINEJOIN},     \
    {TK_OPTION_DOUBLE, "-strokemiterlimit", (char *) NULL, (char *) NULL,   \
        "4.0", -1, Tk_Offset(typeName, style.miterLimit), 0, 0,             \
        PATH_STYLE_OPTION_STROKE_MITERLIMIT},                               \
    {TK_OPTION_DOUBLE, "-strokeopacity", (char *) NULL, (char *) NULL,      \
        "1.0", -1, Tk_Offset(typeName, style.strokeOpacity), 0, 0,          \
        PATH_STYLE_OPTION_STROKE_OPACITY},                                  \
    {TK_OPTION_DOUBLE, "-strokewidth", (char *) NULL, (char *) NULL,        \
        "1.0", -1, Tk_Offset(typeName, style.strokeWidth), 0, 0,    		\
        PATH_STYLE_OPTION_STROKE_WIDTH}
        
#define PATH_OPTION_SPEC_END                                                \
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,            \
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}




