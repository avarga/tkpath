/*
 * Code picked up from various places in the Tk sources.
 *
 * $Id$
 *
 */
 
#include <tcl.h>
#include <tk.h>
#include <tkInt.h>
#include "tkPort.h"
#include "tkInt.h"
#include "tkCanvas.h"



/*
 *--------------------------------------------------------------
 *
 * TkGetDoublePixels --
 *
 *	Given a string, returns the number of pixels corresponding
 *	to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	pixel distance is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
PathTkGetDoublePixels(interp, tkwin, string, doublePtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    Tk_Window tkwin;		/* Window whose screen determines conversion
				 * from centimeters and other absolute
				 * units. */
    CONST char *string;		/* String describing a number of pixels. */
    double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod((char *) string, &end);
    if (end == string) {
	error:
	Tcl_AppendResult(interp, "bad screen distance \"", string,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
	case 0:
	    break;
	case 'c':
	    d *= 10*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'i':
	    d *= 25.4*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'm':
	    d *= WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'p':
	    d *= (25.4/72.0)*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	default:
	    goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    *doublePtr = d;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PathTk_CanvasTagsParseProc --
 *
 *	This procedure is invoked during option processing to handle
 *	"-tags" options for canvas items.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The tags for a given item get replaced by those indicated
 *	in the value argument.
 *
 *--------------------------------------------------------------
 */

int
PathTk_CanvasTagsParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    CONST char *value;			/* Value of option (list of tag
					 * names). */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item (ignored). */
{
    register Tk_Item *itemPtr = (Tk_Item *) widgRec;
    int argc, i;
    CONST char **argv;
    Tk_Uid *newPtr;

    /*
     * Break the value up into the individual tag names.
     */

    if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Make sure that there's enough space in the item to hold the
     * tag names.
     */

    if (itemPtr->tagSpace < argc) {
	newPtr = (Tk_Uid *) ckalloc((unsigned) (argc * sizeof(Tk_Uid)));
	for (i = itemPtr->numTags-1; i >= 0; i--) {
	    newPtr[i] = itemPtr->tagPtr[i];
	}
	if (itemPtr->tagPtr != itemPtr->staticTagSpace) {
	    ckfree((char *) itemPtr->tagPtr);
	}
	itemPtr->tagPtr = newPtr;
	itemPtr->tagSpace = argc;
    }
    itemPtr->numTags = argc;
    for (i = 0; i < argc; i++) {
	itemPtr->tagPtr[i] = Tk_GetUid(argv[i]);
    }
    ckfree((char *) argv);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PathTk_CanvasTagsPrintProc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-tags" configuration
 *	option for canvas items.
 *
 * Results:
 *	The return value is a string describing all the tags for
 *	the item referred to by "widgRec".  In addition, *freeProcPtr
 *	is filled in with the address of a procedure to call to free
 *	the result string when it's no longer needed (or NULL to
 *	indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
PathTk_CanvasTagsPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Ignored. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register Tk_Item *itemPtr = (Tk_Item *) widgRec;

    if (itemPtr->numTags == 0) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return "";
    }
    if (itemPtr->numTags == 1) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return (char *) itemPtr->tagPtr[0];
    }
    *freeProcPtr = TCL_DYNAMIC;
    return Tcl_Merge(itemPtr->numTags, (CONST char **) itemPtr->tagPtr);
}

/*
 *--------------------------------------------------------------
 *
 * PathTkCanvasDashParseProc --
 *
 *	This procedure is invoked during option processing to handle
 *	"-dash", "-activedash" and "-disableddash" options for canvas
 *	objects.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The dash list for a given canvas object gets replaced by
 *	those indicated in the value argument.
 *
 *--------------------------------------------------------------
 */

int
PathTkCanvasDashParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    CONST char *value;			/* Value of option. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
{
    return Tk_GetDash(interp, value, (Tk_Dash *)(widgRec+offset));
}

/*
 *--------------------------------------------------------------
 *
 * PathTkCanvasDashPrintProc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-dash", "-activedash"
 *	and "-disableddash" configuration options for canvas items.
 *
 * Results:
 *	The return value is a string describing all the dash list for
 *	the item referred to by "widgRec"and "offset".  In addition,
 *	*freeProcPtr is filled in with the address of a procedure to
 *	call to free the result string when it's no longer needed (or
 *	NULL to indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
PathTkCanvasDashPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset in record for item. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    Tk_Dash *dash = (Tk_Dash *) (widgRec+offset);
    char *buffer;
    char *p;
    int i = dash->number;

    if (i < 0) {
	i = -i;
	*freeProcPtr = TCL_DYNAMIC;
	buffer = (char *) ckalloc((unsigned int) (i+1));
	p = (i > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
	memcpy(buffer, p, (unsigned int) i);
	buffer[i] = 0;
	return buffer;
    } else if (!i) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return "";
    }
    buffer = (char *)ckalloc((unsigned int) (4*i));
    *freeProcPtr = TCL_DYNAMIC;

    p = (i > (int)sizeof(char *)) ? dash->pattern.pt : dash->pattern.array;
    sprintf(buffer, "%d", *p++ & 0xff);
    while(--i) {
	sprintf(buffer+strlen(buffer), " %d", *p++ & 0xff);
    }
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * PathTkOffsetParseProc --
 *
 *	Converts the offset of a stipple or tile into the Tk_TSOffset structure.
 *
 *----------------------------------------------------------------------
 */

int
PathTkOffsetParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window on same display as tile */
    CONST char *value;		/* Name of image */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
{
    Tk_TSOffset *offsetPtr = (Tk_TSOffset *)(widgRec + offset);
    Tk_TSOffset tsoffset;
    CONST char *q, *p;
    int result;

    if ((value == NULL) || (*value == 0)) {
	tsoffset.flags = TK_OFFSET_CENTER|TK_OFFSET_MIDDLE;
	goto goodTSOffset;
    }
    tsoffset.flags = 0;
    p = value;

    switch(value[0]) {
	case '#':
	    if (((int)clientData) & TK_OFFSET_RELATIVE) {
		tsoffset.flags = TK_OFFSET_RELATIVE;
		p++; break;
	    }
	    goto badTSOffset;
	case 'e':
	    switch(value[1]) {
		case '\0':
		    tsoffset.flags = TK_OFFSET_RIGHT|TK_OFFSET_MIDDLE;
		    goto goodTSOffset;
		case 'n':
		    if (value[2]!='d' || value[3]!='\0') {goto badTSOffset;}
		    tsoffset.flags = INT_MAX;
		    goto goodTSOffset;
	    }
	case 'w':
	    if (value[1] != '\0') {goto badTSOffset;}
	    tsoffset.flags = TK_OFFSET_LEFT|TK_OFFSET_MIDDLE;
	    goto goodTSOffset;
	case 'n':
	    if ((value[1] != '\0') && (value[2] != '\0')) {
		goto badTSOffset;
	    }
	    switch(value[1]) {
		case '\0': tsoffset.flags = TK_OFFSET_CENTER|TK_OFFSET_TOP;
			   goto goodTSOffset;
		case 'w': tsoffset.flags = TK_OFFSET_LEFT|TK_OFFSET_TOP;
			   goto goodTSOffset;
		case 'e': tsoffset.flags = TK_OFFSET_RIGHT|TK_OFFSET_TOP;
			   goto goodTSOffset;
	    }
	    goto badTSOffset;
	case 's':
	    if ((value[1] != '\0') && (value[2] != '\0')) {
		goto badTSOffset;
	    }
	    switch(value[1]) {
		case '\0': tsoffset.flags = TK_OFFSET_CENTER|TK_OFFSET_BOTTOM;
			   goto goodTSOffset;
		case 'w': tsoffset.flags = TK_OFFSET_LEFT|TK_OFFSET_BOTTOM;
			   goto goodTSOffset;
		case 'e': tsoffset.flags = TK_OFFSET_RIGHT|TK_OFFSET_BOTTOM;
			   goto goodTSOffset;
	    }
	    goto badTSOffset;
	case 'c':
	    if (strncmp(value, "center", strlen(value)) != 0) {
		goto badTSOffset;
	    }
	    tsoffset.flags = TK_OFFSET_CENTER|TK_OFFSET_MIDDLE;
	    goto goodTSOffset;
    }
    if ((q = strchr(p,',')) == NULL) {
	if (((int)clientData) & TK_OFFSET_INDEX) {
	    if (Tcl_GetInt(interp, (char *) p, &tsoffset.flags) != TCL_OK) {
		Tcl_ResetResult(interp);
		goto badTSOffset;
	    }
	    tsoffset.flags |= TK_OFFSET_INDEX;
	    goto goodTSOffset;
	}
	goto badTSOffset;
    }
    *((char *) q) = 0;
    result = Tk_GetPixels(interp, tkwin, (char *) p, &tsoffset.xoffset);
    *((char *) q) = ',';
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_GetPixels(interp, tkwin, (char *) q+1, &tsoffset.yoffset) != TCL_OK) {
	return TCL_ERROR;
    }


goodTSOffset:
    /* below is a hack to allow the stipple/tile offset to be stored
     * in the internal tile structure. Most of the times, offsetPtr
     * is a pointer to an already existing tile structure. However
     * if this structure is not already created, we must do it
     * with Tk_GetTile()!!!!;
     */

    memcpy(offsetPtr,&tsoffset, sizeof(Tk_TSOffset));
    return TCL_OK;

badTSOffset:
    Tcl_AppendResult(interp, "bad offset \"", value,
	    "\": expected \"x,y\"", (char *) NULL);
    if (((int) clientData) & TK_OFFSET_RELATIVE) {
	Tcl_AppendResult(interp, ", \"#x,y\"", (char *) NULL);
    }
    if (((int) clientData) & TK_OFFSET_INDEX) {
	Tcl_AppendResult(interp, ", <index>", (char *) NULL);
    }
    Tcl_AppendResult(interp, ", n, ne, e, se, s, sw, w, nw, or center",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PathTkOffsetPrintProc --
 *
 *	Returns the offset of the tile.
 *
 * Results:
 *	The offset of the tile is returned.
 *
 *----------------------------------------------------------------------
 */

char *
PathTkOffsetPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Tk_TSOffset *offsetPtr = (Tk_TSOffset *)(widgRec + offset);
    char *p, *q;

    if ((offsetPtr->flags) & TK_OFFSET_INDEX) {
	if ((offsetPtr->flags) >= INT_MAX) {
	    return "end";
	}
	p = (char *) ckalloc(32);
	sprintf(p, "%d",(offsetPtr->flags & (~TK_OFFSET_INDEX)));
	*freeProcPtr = TCL_DYNAMIC;
	return p;
    }
    if ((offsetPtr->flags) & TK_OFFSET_TOP) {
	if ((offsetPtr->flags) & TK_OFFSET_LEFT) {
	    return "nw";
	} else if ((offsetPtr->flags) & TK_OFFSET_CENTER) {
	    return "n";
	} else if ((offsetPtr->flags) & TK_OFFSET_RIGHT) {
	    return "ne";
	}
    } else if ((offsetPtr->flags) & TK_OFFSET_MIDDLE) {
	if ((offsetPtr->flags) & TK_OFFSET_LEFT) {
	    return "w";
	} else if ((offsetPtr->flags) & TK_OFFSET_CENTER) {
	    return "center";
	} else if ((offsetPtr->flags) & TK_OFFSET_RIGHT) {
	    return "e";
	}
    } else if ((offsetPtr->flags) & TK_OFFSET_BOTTOM) {
	if ((offsetPtr->flags) & TK_OFFSET_LEFT) {
	    return "sw";
	} else if ((offsetPtr->flags) & TK_OFFSET_CENTER) {
	    return "s";
	} else if ((offsetPtr->flags) & TK_OFFSET_RIGHT) {
	    return "se";
	}
    } 
    q = p = (char *) ckalloc(32);
    if ((offsetPtr->flags) & TK_OFFSET_RELATIVE) {
	*q++ = '#';
    }
    sprintf(q, "%d,%d",offsetPtr->xoffset, offsetPtr->yoffset);
    *freeProcPtr = TCL_DYNAMIC;
    return p;
}

/*
 *----------------------------------------------------------------------
 *
 * PathTkPixelParseProc --
 *
 *	Converts the name of an image into a tile.
 *
 *----------------------------------------------------------------------
 */

int
PathTkPixelParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* if non-NULL, negative values are
				 * allowed as well */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window on same display as tile */
    CONST char *value;		/* Name of image */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
{
    double *doublePtr = (double *)(widgRec + offset);
    int result;

    result = PathTkGetDoublePixels(interp, tkwin, value, doublePtr);

    if ((result == TCL_OK) && (clientData == NULL) && (*doublePtr < 0.0)) {
	Tcl_AppendResult(interp, "bad screen distance \"", value,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PathTkPixelPrintProc --
 *
 *	Returns the name of the tile.
 *
 * Results:
 *	The name of the tile is returned.
 *
 *----------------------------------------------------------------------
 */

char *
PathTkPixelPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    double *doublePtr = (double *)(widgRec + offset);
    char *p;

    p = (char *) ckalloc(24);
    Tcl_PrintDouble((Tcl_Interp *) NULL, *doublePtr, p);
    *freeProcPtr = TCL_DYNAMIC;
    return p;
}

/*
 *--------------------------------------------------------------
 *
 * PathTkStateParseProc --
 *
 *	This procedure is invoked during option processing to handle
 *	the "-state" and "-default" options.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The state for a given item gets replaced by the state
 *	indicated in the value argument.
 *
 *--------------------------------------------------------------
 */

int
PathTkStateParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* some flags.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    CONST char *value;			/* Value of option. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
{
    int c;
    int flags = (int)clientData;
    size_t length;

    register Tk_State *statePtr = (Tk_State *) (widgRec + offset);

    if(value == NULL || *value == 0) {
	*statePtr = TK_STATE_NULL;
	return TCL_OK;
    }

    c = value[0];
    length = strlen(value);

    if ((c == 'n') && (strncmp(value, "normal", length) == 0)) {
	*statePtr = TK_STATE_NORMAL;
	return TCL_OK;
    }
    if ((c == 'd') && (strncmp(value, "disabled", length) == 0)) {
	*statePtr = TK_STATE_DISABLED;
	return TCL_OK;
    }
    if ((c == 'a') && (flags&1) && (strncmp(value, "active", length) == 0)) {
	*statePtr = TK_STATE_ACTIVE;
	return TCL_OK;
    }
    if ((c == 'h') && (flags&2) && (strncmp(value, "hidden", length) == 0)) {
	*statePtr = TK_STATE_HIDDEN;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad ", (flags&4)?"-default" : "state",
	    " value \"", value, "\": must be normal",
	    (char *) NULL);
    if (flags&1) {
	Tcl_AppendResult(interp, ", active",(char *) NULL);
    }
    if (flags&2) {
	Tcl_AppendResult(interp, ", hidden",(char *) NULL);
    }
    if (flags&3) {
	Tcl_AppendResult(interp, ",",(char *) NULL);
    }
    Tcl_AppendResult(interp, " or disabled",(char *) NULL);
    *statePtr = TK_STATE_NORMAL;
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * PathTkStatePrintProc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-state"
 *	configuration option.
 *
 * Results:
 *	The return value is a string describing the state for
 *	the item referred to by "widgRec".  In addition, *freeProcPtr
 *	is filled in with the address of a procedure to call to free
 *	the result string when it's no longer needed (or NULL to
 *	indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
PathTkStatePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register Tk_State *statePtr = (Tk_State *) (widgRec + offset);

    if (*statePtr==TK_STATE_NORMAL) {
	return "normal";
    } else if (*statePtr==TK_STATE_DISABLED) {
	return "disabled";
    } else if (*statePtr==TK_STATE_HIDDEN) {
	return "hidden";
    } else if (*statePtr==TK_STATE_ACTIVE) {
	return "active";
    } else {
	return "";
    }
}
