/*
 * tkPathStyle.c --
 *
 *	    This file implements style objects used when drawing paths.
 *      See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2007  Mats Bengtsson
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

#include "tkIntPath.h"
#include "tkPathStyle.h"

static Tcl_HashTable 	*gStyleHashPtr;
static Tk_OptionTable 	gStyleOptionTable;
static int 				gStyleNameUid = 0;
static char 			*kStyleNameBase = "pathstyle";


/*
 * Declarationd for functions local to this file.
 */

static int 		StyleObjCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]);
static char *	StyleCreateAndConfig(Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[]);
static void 	StyleConfigNotify(char *recordPtr, int mask, int objc, Tcl_Obj *CONST objv[]);
static void		StyleFree(Tcl_Interp *interp, char *recordPtr);

/*
 * Custom option support.
 */

/*
 * The -fillgradient custom option.
 */
 
int FillGradientSetOption(
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
            if (HaveGradientStyleWithName(name) != TCL_OK) {
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

Tcl_Obj *
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

void
FillGradientFreeOption(
        ClientData clientData,
        Tk_Window tkwin, 
        char *internalPtr)
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

/*
 * The -matrix custom option.
 */

int MatrixSetOption(
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
    char *list;
    int length;
    Tcl_Obj *valuePtr;
    TMatrix *new;
    
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
            new = (TMatrix *) ckalloc(sizeof(TMatrix));
            if (PathGetTMatrix(interp, list, new) != TCL_OK) {
                ckfree((char *) new);
                return TCL_ERROR;
            }
		} else {
		    new = NULL;
        }
		*((TMatrix **) oldInternalPtr) = *((TMatrix **) internalPtr);
		*((TMatrix **) internalPtr) = new;
    }
    return TCL_OK;
}

Tcl_Obj *
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

void
MatrixRestoreOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,		/* Pointer to storage for value. */
    char *oldInternalPtr)	/* Pointer to old value. */
{
    *(TMatrix **)internalPtr = *(TMatrix **)oldInternalPtr;
}

void
MatrixFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

/*
 * The -strokedasharray custom option.
 */

int DashSetOption(
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

Tcl_Obj *
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

void
DashFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)		/* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        Tk_Dash *dash = (Tk_Dash *) internalPtr;

        if (ABS(dash->number) > sizeof(char *)) {
            ckfree((char *) dash->pattern.pt);
        }
    }
}

PATH_STYLE_CUSTOM_OPTION_RECORDS

/* Just a placeholder for not yet implemented stuff. */
static char *nullST[] = {
    "", (char *) NULL
};

// @@@ BAD I had to duplicate this record here and in tkPathStyle.h.
//     Else I get problems with Tk_Offset and records.

static Tk_OptionSpec styleOptionSpecs[] = {
    {TK_OPTION_COLOR, "-fill", (char *) NULL, (char *) NULL,
        "", -1, Tk_Offset(Tk_PathStyle, fillColor), TK_OPTION_NULL_OK, 0, 
        PATH_STYLE_OPTION_FILL},
	{TK_OPTION_CUSTOM, "-fillgradient", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, gradientFillName),
		TK_OPTION_NULL_OK, (ClientData) &fillGradientCO, 
        PATH_STYLE_OPTION_FILL_GRADIENT},
	{TK_OPTION_STRING_TABLE, "-filloffset", (char *) NULL, (char *) NULL,	/* @@@ TODO */
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, null),
		0, (ClientData) &nullST, PATH_STYLE_OPTION_FILL_OFFSET},
    {TK_OPTION_DOUBLE, "-fillopacity", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, fillOpacity), 0, 0, 
        PATH_STYLE_OPTION_FILL_OPACITY},
    {TK_OPTION_STRING_TABLE, "-fillrule", (char *) NULL, (char *) NULL,
        "nonzero", -1, Tk_Offset(Tk_PathStyle, fillRule), 
        0, (ClientData) fillRuleST, PATH_STYLE_OPTION_FILL_RULE},
	{TK_OPTION_CUSTOM, "-matrix", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, matrixPtr),
		TK_OPTION_NULL_OK, (ClientData) &matrixCO, PATH_STYLE_OPTION_MATRIX},
    {TK_OPTION_COLOR, "-stroke", (char *) NULL, (char *) NULL,
        "black", -1, Tk_Offset(Tk_PathStyle, strokeColor), TK_OPTION_NULL_OK, 0, 
        PATH_STYLE_OPTION_STROKE},
	{TK_OPTION_CUSTOM, "-strokedasharray", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, dash),
		TK_OPTION_NULL_OK, (ClientData) &dashCO, PATH_STYLE_OPTION_STROKE_DASHARRAY},
    {TK_OPTION_STRING_TABLE, "-strokelinecap", (char *) NULL, (char *) NULL,
        "butt", -1, Tk_Offset(Tk_PathStyle, capStyle), 
        0, (ClientData) lineCapST, PATH_STYLE_OPTION_STROKE_LINECAP},
    {TK_OPTION_STRING_TABLE, "-strokelinejoin", (char *) NULL, (char *) NULL,
        "round", -1, Tk_Offset(Tk_PathStyle, joinStyle), 
        0, (ClientData) lineJoinST, PATH_STYLE_OPTION_STROKE_LINEJOIN},
    {TK_OPTION_DOUBLE, "-strokemiterlimit", (char *) NULL, (char *) NULL,
        "4.0", -1, Tk_Offset(Tk_PathStyle, miterLimit), 0, 0, 
        PATH_STYLE_OPTION_STROKE_MITERLIMIT},
	{TK_OPTION_STRING_TABLE, "-strokeoffset", (char *) NULL, (char *) NULL,	/* @@@ TODO */
		(char *) NULL, -1, Tk_Offset(Tk_PathStyle, null),
		0, (ClientData) &nullST, PATH_STYLE_OPTION_STROKE_OFFSET},
    {TK_OPTION_DOUBLE, "-strokeopacity", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, strokeOpacity), 0, 0, 
        PATH_STYLE_OPTION_STROKE_OPACITY},
    {TK_OPTION_DOUBLE, "-strokewidth", (char *) NULL, (char *) NULL,
        "1.0", -1, Tk_Offset(Tk_PathStyle, strokeWidth), 0, 0, 
        PATH_STYLE_OPTION_STROKE_WIDTH},
    
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

    Tcl_CreateObjCommand(interp, "::tkpath::style",
            StyleObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPathConfigStyle --
 *
 *		Parses a list of Tcl objects to an already allocated Tk_PathStyle.
 *
 * Results:
 *		Standard Tcl result
 *
 * Side effects:
 *		Options allocated. Use Tk_FreeConfigOptions when finished.
 *
 *----------------------------------------------------------------------
 */

int
TkPathConfigStyle(Tcl_Interp* interp, Tk_PathStyle *stylePtr, int objc, Tcl_Obj* CONST objv[])
{
    Tk_Window tkwin = Tk_MainWindow(interp);    
	stylePtr->optionTable = gStyleOptionTable; 
    if (Tk_InitOptions(interp, (char *)stylePtr, gStyleOptionTable, tkwin) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tk_SetOptions(interp, (char *)stylePtr, gStyleOptionTable, 	
            objc, objv, tkwin, NULL, NULL) != TCL_OK) {
        Tk_FreeConfigOptions((char *)stylePtr, gStyleOptionTable, NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
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
            StyleCreateAndConfig, StyleConfigNotify, StyleFree);
}


static char *
StyleCreateAndConfig(
        Tcl_Interp *interp, char *name, int objc, Tcl_Obj *CONST objv[])
{
	Tk_PathStyle *stylePtr;

	stylePtr = (Tk_PathStyle *) ckalloc(sizeof(Tk_PathStyle));
	memset(stylePtr, '\0', sizeof(Tk_PathStyle));

    /* Fill in defaults */
    TkPathCreateStyle(stylePtr);
    
    /*
     * Create the option table for this class.  If it has already
     * been created, the cached pointer will be returned.
     */
	stylePtr->optionTable = gStyleOptionTable; 
	stylePtr->name = Tk_GetUid(name);
    
    return (char *) stylePtr;
}

static void 
StyleConfigNotify(char *recordPtr, int mask, int objc, Tcl_Obj *CONST objv[])
{
    Tk_PathStyle *stylePtr = (Tk_PathStyle *) recordPtr;

    /* Let mask be the cumalative options set. */
    stylePtr->mask |= mask;
}

static void
StyleFree(Tcl_Interp *interp, char *recordPtr) 
{
    TkPathDeleteStyle(Tk_Display(Tk_MainWindow(interp)), (Tk_PathStyle *) recordPtr);
    ckfree(recordPtr);
}

int
PathStyleHaveWithName(CONST char *name)
{
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(gStyleHashPtr, name);
	if (hPtr == NULL) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

static void
CopyXColor(Tk_Window tkwin, XColor **dstPtrPtr, XColor *srcPtr)
{
    XColor *dstPtr;
    XColor *colorPtr = NULL;
    
    dstPtr = *dstPtrPtr;
    if ((dstPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else if (dstPtr == NULL) {
        colorPtr = Tk_GetColorByValue(tkwin, srcPtr);
    } else {
        Tk_FreeColor(dstPtr);
        colorPtr = Tk_GetColorByValue(tkwin, srcPtr);
    }
    *dstPtrPtr = colorPtr;
}

static void
CopyTMatrix(TMatrix **dstPtrPtr, TMatrix *srcPtr)
{
    TMatrix *dstPtr;
    TMatrix *matrixPtr = NULL;
    
    dstPtr = *dstPtrPtr;
    if ((dstPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else if (dstPtr == NULL) {
        matrixPtr = (TMatrix *) ckalloc(sizeof(TMatrix));
        *matrixPtr = *srcPtr;
    } else {
        *matrixPtr = *srcPtr;
    }
    *dstPtrPtr = matrixPtr;
}

static void
CopyTkDash(Tk_Dash *dstPtr, Tk_Dash *srcPtr)
{

    /* @@@ TODO */
}

static void
CopyOptionString(char **dstPtrPtr, char *srcPtr)
{
    if ((*dstPtrPtr == NULL) && (srcPtr == NULL)) {
        /* empty */
    } else if (*dstPtrPtr == NULL) {
        *dstPtrPtr = (char *) ckalloc(strlen(srcPtr) + 1);
        strcpy(*dstPtrPtr, srcPtr);
    } else {
        ckfree(*dstPtrPtr);
        *dstPtrPtr = (char *) ckalloc(strlen(srcPtr) + 1);
        strcpy(*dstPtrPtr, srcPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PathStyleMergeStyles --
 *
 *		Copies the style options set in 'styleName' to the one in 'stylePtr'.
 *		Depending on 'flags' only fill or stroke options set.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Variables in 'stylePtr' set.
 *
 *--------------------------------------------------------------
 */

void
PathStyleMergeStyles(Tk_Window tkwin, Tk_PathStyle *stylePtr, CONST char *styleName, long flags)
{
    int mask;
	Tcl_HashEntry *hPtr;
    Tk_PathStyle *srcStylePtr;

    if (styleName == NULL) {
        return;
    }
	hPtr = Tcl_FindHashEntry(gStyleHashPtr, styleName);
	if (hPtr == NULL) {
        return;
    }
    srcStylePtr = (Tk_PathStyle *) Tcl_GetHashValue(hPtr);

    /*
     * Go through all options set in srcStylePtr and merge
     * these into stylePtr.
     */
    mask = srcStylePtr->mask;
    if (!(flags & kPathMergeStyleNotFill)) {
        if (mask & PATH_STYLE_OPTION_FILL) {
            CopyXColor(tkwin, &(stylePtr->fillColor), srcStylePtr->fillColor);
        }
        if (mask & PATH_STYLE_OPTION_FILL_GRADIENT) {
            CopyOptionString(&(stylePtr->gradientFillName), srcStylePtr->gradientFillName);
        }
        if (mask & PATH_STYLE_OPTION_FILL_OFFSET) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_FILL_OPACITY) {
            stylePtr->fillOpacity = srcStylePtr->fillOpacity;
        }
        if (mask & PATH_STYLE_OPTION_FILL_RULE) {
            stylePtr->fillRule = srcStylePtr->fillRule;
        }
        if (mask & PATH_STYLE_OPTION_FILL_STIPPLE) {
            /* @@@ TODO */
        }
    }
    if (mask & PATH_STYLE_OPTION_MATRIX) {
        CopyTMatrix(&(stylePtr->matrixPtr), srcStylePtr->matrixPtr);
    }
    if (!(flags & kPathMergeStyleNotStroke)) {
        if (mask & PATH_STYLE_OPTION_STROKE) {
            CopyXColor(tkwin, &(stylePtr->strokeColor), srcStylePtr->strokeColor);
        }
        if (mask & PATH_STYLE_OPTION_STROKE_DASHARRAY) {
            CopyTkDash(&(stylePtr->dash), &(srcStylePtr->dash));
        }
        if (mask & PATH_STYLE_OPTION_STROKE_LINECAP) {
            stylePtr->capStyle = srcStylePtr->capStyle;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_LINEJOIN) {
            stylePtr->joinStyle = srcStylePtr->joinStyle;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_MITERLIMIT) {
            stylePtr->miterLimit = srcStylePtr->miterLimit;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_OFFSET) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_STROKE_OPACITY) {
            stylePtr->strokeOpacity = srcStylePtr->strokeOpacity;
        }
        if (mask & PATH_STYLE_OPTION_STROKE_STIPPLE) {
            /* @@@ TODO */
        }
        if (mask & PATH_STYLE_OPTION_STROKE_WIDTH) {
            stylePtr->strokeWidth = srcStylePtr->strokeWidth;
        }
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkPathCreateStyle
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
TkPathCreateStyle(Tk_PathStyle *style)
{
	memset(style, '\0', sizeof(Tk_PathStyle));

    style->mask = 0;
    style->strokeGC = None;
    style->strokeColor = NULL;
    style->strokeWidth = 1.0;
    style->strokeOpacity = 1.0;
    style->offset = 0;
    style->dash.number = 0;
    style->capStyle = CapButt;
    style->joinStyle = JoinRound;
    style->gradientStrokeName = NULL;    

    style->fillGC = None;
    style->fillColor = NULL;
    style->fillOpacity = 1.0;
    style->fillRule = WindingRule;
    style->gradientFillName = NULL;    
    style->matrixPtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * TkPathDeleteStyle
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
TkPathDeleteStyle(Display *display, Tk_PathStyle *style)
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

    if (style->fillGC != None) {
        Tk_FreeGC(display, style->fillGC);
    }
    if (style->fillColor != NULL) {
        Tk_FreeColor(style->fillColor);
    }
    if (style->matrixPtr != NULL) {
        ckfree((char *) style->matrixPtr);
    }
}

/*-------------------------------------------------------------------*/

