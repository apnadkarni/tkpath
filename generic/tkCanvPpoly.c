/*
 * tkCanvPpolygon.c --
 *
 *	This file implements polygon and polyline canvas items modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007-2008  Mats Bengtsson
 *
 */

#include "tkIntPath.h"
#include "tkpCanvas.h"
#include "tkCanvPathUtil.h"
#include "tkCanvArrow.h"
#include "tkPathStyle.h"

/*
 * The structure below defines the record for each path item.
 */

typedef struct PpolyItem  {
    Tk_PathItemEx headerEx; /* Generic stuff that's the same for all
                             * path types.  MUST BE FIRST IN STRUCTURE. */
    char type;		    /* Polyline or polygon. */
    PathAtom *atomPtr;
    int maxNumSegments;	    /* Max number of straight segments (for subpath)
			     * needed for Area and Point functions. */
    ArrowDescr startarrow;
    ArrowDescr endarrow;
} PpolyItem;

enum {
    kPpolyTypePolyline,
    kPpolyTypePolygon
};


/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePpolyBbox(Tk_PathCanvas canvas, PpolyItem *ppolyPtr);
static int	ConfigurePpoly(Tcl_Interp *interp, Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Tcl_Size objc,
                        Tcl_Obj *const objv[], int flags);
static int	CoordsForPolygonline(Tcl_Interp *interp, Tk_PathCanvas canvas,
			int closed, Tcl_Size objc, Tcl_Obj *const objv[],
			PathAtom **atomPtrPtr, Tcl_Size *lenPtr);
static int	CreateAny(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[], char type);
static int	CreatePolyline(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static int	CreatePpolygon(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static void	DeletePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display);
static void	DisplayPpoly(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static void	PpolyBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask);
static int	PpolyCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
                        Tcl_Size objc, Tcl_Obj *const objv[]);
static int	PpolyToArea(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *rectPtr);
static int	PpolyToPdf(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			Tcl_Size objc, Tcl_Obj *const objv[], int prepass);
static double	PpolyToPoint(Tk_PathCanvas canvas,
                        Tk_PathItem *itemPtr, double *coordPtr);
#ifndef TKP_NO_POSTSCRIPT
static int	PpolyToPostscript(Tcl_Interp *interp,
                        Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int prepass);
#endif
static void	ScalePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double originX, double originY,
			double scaleX, double scaleY);
static void	TranslatePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
			int compensate, double deltaX, double deltaY);
static int      ConfigureArrows(Tk_PathCanvas canvas, PpolyItem *ppolyPtr);

PATH_STYLE_CUSTOM_OPTION_RECORDS
PATH_CUSTOM_OPTION_TAGS
PATH_OPTION_STRING_TABLES_FILL
PATH_OPTION_STRING_TABLES_STROKE
PATH_OPTION_STRING_TABLES_STATE

static Tk_OptionSpec optionSpecsPolyline[] = {
    PATH_OPTION_SPEC_CORE(Tk_PathItemEx),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(Tk_PathItemEx, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(Tk_PathItemEx),
    PATH_OPTION_SPEC_STYLE_STROKE(Tk_PathItemEx, "black"),
    PATH_OPTION_SPEC_STARTARROW_GRP(PpolyItem),
    PATH_OPTION_SPEC_ENDARROW_GRP(PpolyItem),
    PATH_OPTION_SPEC_END
};

static Tk_OptionSpec optionSpecsPpolygon[] = {
    PATH_OPTION_SPEC_CORE(Tk_PathItemEx),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_STYLE_FILL(Tk_PathItemEx, ""),
    PATH_OPTION_SPEC_STYLE_MATRIX(Tk_PathItemEx),
    PATH_OPTION_SPEC_STYLE_STROKE(Tk_PathItemEx, "black"),
    PATH_OPTION_SPEC_END
};

/*
 * The structures below defines the 'polyline' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPolylineType = {
    "polyline",				/* name */
    sizeof(PpolyItem),			/* itemSize */
    CreatePolyline,			/* createProc */
    optionSpecsPolyline,		/* OptionSpecs */
    ConfigurePpoly,			/* configureProc */
    PpolyCoords,			/* coordProc */
    DeletePpoly,			/* deleteProc */
    DisplayPpoly,			/* displayProc */
    0,					/* flags */
    PpolyBbox,				/* bboxProc */
    PpolyToPoint,			/* pointProc */
    PpolyToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    PpolyToPostscript,			/* postscriptProc */
#endif
    PpolyToPdf,				/* pdfProc */
    ScalePpoly,				/* scaleProc */
    TranslatePpoly,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};

Tk_PathItemType tkPpolygonType = {
    "ppolygon",				/* name */
    sizeof(PpolyItem),			/* itemSize */
    CreatePpolygon,			/* createProc */
    optionSpecsPpolygon,		/* OptionSpecs */
    ConfigurePpoly,			/* configureProc */
    PpolyCoords,			/* coordProc */
    DeletePpoly,			/* deleteProc */
    DisplayPpoly,			/* displayProc */
    0,					/* flags */
    PpolyBbox,				/* bboxProc */
    PpolyToPoint,			/* pointProc */
    PpolyToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    PpolyToPostscript,			/* postscriptProc */
#endif
    PpolyToPdf,				/* pdfProc */
    ScalePpoly,				/* scaleProc */
    TranslatePpoly,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};

static int
CreatePolyline(Tcl_Interp *interp, Tk_PathCanvas canvas,
    struct Tk_PathItem *itemPtr, Tcl_Size objc, Tcl_Obj *const objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kPpolyTypePolyline);
}

static int
CreatePpolygon(Tcl_Interp *interp, Tk_PathCanvas canvas,
    struct Tk_PathItem *itemPtr, Tcl_Size objc, Tcl_Obj *const objv[])
{
    return CreateAny(interp, canvas, itemPtr, objc, objv, kPpolyTypePolygon);
}

static int
CreateAny(Tcl_Interp *interp, Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], char type)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &ppolyPtr->headerEx;
    Tk_OptionTable optionTable;
    Tcl_Size	i, len;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    TkPathInitStyle(&itemExPtr->style);
    itemExPtr->canvas = canvas;
    itemExPtr->styleObj = NULL;
    itemExPtr->styleInst = NULL;
    ppolyPtr->atomPtr = NULL;
    ppolyPtr->type = type;
    itemPtr->bbox = NewEmptyPathRect();
    itemPtr->totalBbox = NewEmptyPathRect();
    ppolyPtr->maxNumSegments = 0;
    TkPathArrowDescrInit(&ppolyPtr->startarrow);
    TkPathArrowDescrInit(&ppolyPtr->endarrow);

    if (ppolyPtr->type == kPpolyTypePolyline) {
	optionTable = Tk_CreateOptionTable(interp, optionSpecsPolyline);
    } else {
	optionTable = Tk_CreateOptionTable(interp, optionSpecsPpolygon);
    }
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) ppolyPtr, optionTable,
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    if (CoordsForPolygonline(interp, canvas,
	    (ppolyPtr->type == kPpolyTypePolyline) ? 0 : 1,
	    i, objv, &(ppolyPtr->atomPtr), &len) != TCL_OK) {
        goto error;
    }
    ppolyPtr->maxNumSegments = len;

    if (ConfigurePpoly(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    /*
     * NB: We must unlink the item here since the TkPathCanvasItemExConfigure()
     *     link it to the root by default.
     */
    TkPathCanvasItemDetach(itemPtr);
    DeletePpoly(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int
PpolyCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[])
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    int closed;
    Tcl_Size len;

    closed = (ppolyPtr->type == kPpolyTypePolyline) ? 0 : 1;
    if (CoordsForPolygonline(interp, canvas, closed, objc, objv,
            &(ppolyPtr->atomPtr), &len) != TCL_OK) {
        return TCL_ERROR;
    }
    ppolyPtr->maxNumSegments = len;
    ConfigureArrows(canvas, ppolyPtr);
    ComputePpolyBbox(canvas, ppolyPtr);
    return TCL_OK;
}

void
ComputePpolyBbox(Tk_PathCanvas canvas, PpolyItem *ppolyPtr)
{
    Tk_PathItemEx *itemExPtr = &ppolyPtr->headerEx;
    Tk_PathItem *itemPtr = &itemExPtr->header;
    Tk_PathStyle style;
    Tk_PathState state = itemExPtr->header.state;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if ((ppolyPtr->atomPtr == NULL) || (state == TK_PATHSTATE_HIDDEN)) {
        itemExPtr->header.x1 = itemExPtr->header.x2 =
        itemExPtr->header.y1 = itemExPtr->header.y2 = -1;
        return;
    }
    style = TkPathCanvasInheritStyle(itemPtr, kPathMergeStyleNotFill);
    itemPtr->bbox = GetGenericBarePathBbox(ppolyPtr->atomPtr);
    IncludeArrowPointsInRect(&itemPtr->bbox, &ppolyPtr->startarrow);
    IncludeArrowPointsInRect(&itemPtr->bbox, &ppolyPtr->endarrow);
    itemPtr->totalBbox = GetGenericPathTotalBboxFromBare(ppolyPtr->atomPtr,
            &style, &itemPtr->bbox);
    SetGenericPathHeaderBbox(&itemExPtr->header, style.matrixPtr,
	    &itemPtr->totalBbox);
    TkPathCanvasFreeInheritedStyle(&style);
}

/*--------------------------------------------------------------
 *
 * ConfigureArrows --
 *
 *  If arrowheads have been requested for a line, this function makes
 *  arrangements for the arrowheads.
 *
 * Results:
 *  Always returns TCL_OK.
 *
 * Side effects:
 *  Information in ppolyPtr is set up for one or two arrowheads. The
 *  startarrowPtr and endarrowPtr polygons are allocated and initialized,
 *  if need be, and the end points of the line are adjusted so that a
 *  thick line doesn't stick out past the arrowheads.
 *
 *--------------------------------------------------------------
 */

typedef PathPoint *PathPointPtr;

static int
ConfigureArrows(Tk_PathCanvas canvas, PpolyItem *ppolyPtr)
{
    PathPoint *pfirstp;
    PathPoint psecond;
    PathPoint ppenult;
    PathPoint *plastp;

    int error = GetSegmentsFromPathAtomList(ppolyPtr->atomPtr,
			&pfirstp, &psecond, &ppenult, &plastp);

    if (error == TCL_OK) {
        PathPoint pfirst = *pfirstp;
        PathPoint plast = *plastp;
        Tk_PathStyle *lineStyle = &ppolyPtr->headerEx.style;
        int isOpen = lineStyle->fill==NULL &&
		((pfirst.x != plast.x) || (pfirst.y != plast.y));

        TkPathPreconfigureArrow(&pfirst, &ppolyPtr->startarrow);
        TkPathPreconfigureArrow(&plast, &ppolyPtr->endarrow);

        *pfirstp = TkPathConfigureArrow(pfirst, psecond,
			&ppolyPtr->startarrow, lineStyle, isOpen);
        *plastp = TkPathConfigureArrow(plast, ppenult, &ppolyPtr->endarrow,
			lineStyle, isOpen);
    } else {
        TkPathFreeArrow(&ppolyPtr->startarrow);
        TkPathFreeArrow(&ppolyPtr->endarrow);
    }

    return TCL_OK;
}

static int
ConfigurePpoly(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], int flags)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &ppolyPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;
    Tk_Window tkwin;
    Tk_SavedOptions savedOptions;
    Tcl_Obj *errorResult = NULL;
    int mask, error;

    tkwin = Tk_PathCanvasTkwin(canvas);
    for (error = 0; error <= 1; error++) {
	if (!error) {
	    if (Tk_SetOptions(interp, (char *) ppolyPtr, itemPtr->optionTable,
		    objc, objv, tkwin, &savedOptions, &mask) != TCL_OK) {
		continue;
	    }
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);
	}
	if (TkPathCanvasItemExConfigure(interp, canvas, itemExPtr, mask) != TCL_OK) {
	    continue;
	}

	/*
	 * If we reach this on the first pass we are OK and continue below.
	 */
	break;
    }
    if (!error) {
	Tk_FreeSavedOptions(&savedOptions);
	stylePtr->mask |= mask;
    }
    stylePtr->strokeOpacity = MAX(0.0, MIN(1.0, stylePtr->strokeOpacity));

    ConfigureArrows(canvas, ppolyPtr);

    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
	ComputePpolyBbox(canvas, ppolyPtr);
	return TCL_OK;
    }
}

static void
DeletePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &ppolyPtr->headerEx;
    Tk_PathStyle *stylePtr = &itemExPtr->style;

    if (stylePtr->fill != NULL) {
	TkPathFreePathColor(stylePtr->fill);
    }
    if (itemExPtr->styleInst != NULL) {
	TkPathFreeStyle(itemExPtr->styleInst);
    }
    if (ppolyPtr->atomPtr != NULL) {
        TkPathFreeAtoms(ppolyPtr->atomPtr);
        ppolyPtr->atomPtr = NULL;
    }
    TkPathFreeArrow(&ppolyPtr->startarrow);
    TkPathFreeArrow(&ppolyPtr->endarrow);
    Tk_FreeConfigOptions((char *) itemPtr, itemPtr->optionTable,
			 Tk_PathCanvasTkwin(canvas));
}

static void
DisplayPpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display,
    Drawable drawable, int x, int y, int width, int height)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle style;

    style = TkPathCanvasInheritStyle(itemPtr, 0);
    TkPathDrawPath(Tk_PathCanvasTkwin(canvas), drawable, ppolyPtr->atomPtr,
	    &style, &m, &itemPtr->bbox);
    /*
     * Display arrowheads, if they are wanted.
     */
    DisplayArrow(canvas, drawable, &ppolyPtr->startarrow, &style, &m,
	    &itemPtr->bbox);
    DisplayArrow(canvas, drawable, &ppolyPtr->endarrow, &style, &m,
	    &itemPtr->bbox);
    TkPathCanvasFreeInheritedStyle(&style);
}

static void
PpolyBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    ComputePpolyBbox(canvas, ppolyPtr);
}

static double
PpolyToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathStyle style;
    double dist;
    long flags;

    flags = (ppolyPtr->type == kPpolyTypePolyline) ?
	    kPathMergeStyleNotFill : 0;
    style = TkPathCanvasInheritStyle(itemPtr, flags);
    dist = GenericPathToPoint(canvas, itemPtr, &style, ppolyPtr->atomPtr,
            ppolyPtr->maxNumSegments, pointPtr);
    TkPathCanvasFreeInheritedStyle(&style);
    return dist;
}

static int
PpolyToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathStyle style;
    int area;
    long flags;

    flags = (ppolyPtr->type == kPpolyTypePolyline) ?
	    kPathMergeStyleNotFill : 0;
    style = TkPathCanvasInheritStyle(itemPtr, flags);
    area = GenericPathToArea(canvas, itemPtr, &style,
            ppolyPtr->atomPtr, ppolyPtr->maxNumSegments, areaPtr);
    TkPathCanvasFreeInheritedStyle(&style);
    return area;
}

static int
PpolyToPdf(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], int prepass)
{
    Tk_PathStyle style;
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;
    Tk_PathState state = itemPtr->state;
    int result;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if ((ppolyPtr->atomPtr == NULL) || (state == TK_PATHSTATE_HIDDEN)) {
	return TCL_OK;
    }
    style = TkPathCanvasInheritStyle(itemPtr, 0);
    result = TkPathPdf(interp, ppolyPtr->atomPtr, &style, &itemPtr->bbox,
		       objc, objv);
    if (result == TCL_OK) {
	result = TkPathPdfArrow(interp, &ppolyPtr->startarrow, &style);
	if (result == TCL_OK) {
	    result = TkPathPdfArrow(interp, &ppolyPtr->endarrow, &style);
	}
    }
    TkPathCanvasFreeInheritedStyle(&style);
    return result;
}

#ifndef TKP_NO_POSTSCRIPT
static int
PpolyToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas,
    Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}
#endif

static void
ScalePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int compensate,
    double originX, double originY, double scaleX, double scaleY)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;

    CompensateScale(itemPtr, compensate, &originX, &originY, &scaleX, &scaleY);

    ScalePathAtoms(ppolyPtr->atomPtr, originX, originY, scaleX, scaleY);
    ScalePathRect(&itemPtr->bbox, originX, originY, scaleX, scaleY);
    TkPathScaleArrow(&ppolyPtr->startarrow, originX, originY, scaleX, scaleY);
    TkPathScaleArrow(&ppolyPtr->endarrow, originX, originY, scaleX, scaleY);
    ConfigureArrows(canvas, ppolyPtr);
    ScaleItemHeader(itemPtr, originX, originY, scaleX, scaleY);
}

static void
TranslatePpoly(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int compensate,
    double deltaX, double deltaY)
{
    PpolyItem *ppolyPtr = (PpolyItem *) itemPtr;

    CompensateTranslate(itemPtr, compensate, &deltaX, &deltaY);

    TranslatePathAtoms(ppolyPtr->atomPtr, deltaX, deltaY);
    TranslatePathRect(&itemPtr->bbox, deltaX, deltaY);
    TkPathTranslateArrow(&ppolyPtr->startarrow, deltaX, deltaY);
    TkPathTranslateArrow(&ppolyPtr->endarrow, deltaX, deltaY);
    TranslateItemHeader(itemPtr, deltaX, deltaY);
}

/*
 *--------------------------------------------------------------
 *
 * CoordsForPolygonline --
 *
 *		Used as coordProc for polyline and polygon items.
 *
 * Results:
 *		Standard tcl result.
 *
 * Side effects:
 *		May store new atoms in atomPtrPtr and max number of points
 *		in lenPtr.
 *
 *--------------------------------------------------------------
 */

int
CoordsForPolygonline(
    Tcl_Interp *interp,
    Tk_PathCanvas canvas,
    int closed,				/* Polyline (0) or polygon (1) */
    Tcl_Size objc,
    Tcl_Obj *const objv[],
    PathAtom **atomPtrPtr,
    Tcl_Size *lenPtr)
{
    PathAtom *atomPtr = *atomPtrPtr;

    if (objc == 0) {
        Tcl_Obj *obj = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

        while (atomPtr != NULL) {
            switch (atomPtr->type) {
                case PATH_ATOM_M: {
                    MoveToAtom *move = (MoveToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(move->y));
                    break;
                }
                case PATH_ATOM_L: {
                    LineToAtom *line = (LineToAtom *) atomPtr;
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->x));
                    Tcl_ListObjAppendElement(interp, obj, Tcl_NewDoubleObj(line->y));
                    break;
                }
                case PATH_ATOM_Z: {

                    break;
                }
                default: {
                    /* empty */
                }
            }
            atomPtr = atomPtr->nextPtr;
        }
        Tcl_SetObjResult(interp, obj);
        *lenPtr = 0;
        return TCL_OK;
    }
    if (objc == 1) {
        if (Tcl_ListObjGetElements(interp, objv[0], &objc,
            (Tcl_Obj ***) &objv) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    if (objc & 1) {
        Tcl_SetObjResult(interp,
            Tcl_ObjPrintf("wrong # coordinates: expected an even number, got %"
                          TCL_SIZE_MODIFIER "d", objc));
        return TCL_ERROR;
    } else if (objc < 4) {
        Tcl_SetObjResult(interp,
            Tcl_ObjPrintf("wrong # coordinates: expected at least 4, got %"
                                       TCL_SIZE_MODIFIER "d", objc));
        return TCL_ERROR;
    } else {
        Tcl_Size i;
        double	x, y;
        double	firstX = 0.0, firstY = 0.0;
        PathAtom *firstAtomPtr = NULL;

        /*
        * Free any old stuff.
        */
        if (atomPtr != NULL) {
            TkPathFreeAtoms(atomPtr);
            atomPtr = NULL;
        }
        for (i = 0; i < objc; i += 2) {
            if (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[i], &x) != TCL_OK) {
                /* @@@ error recovery? */
                return TCL_ERROR;
            }
            if (Tk_PathCanvasGetCoordFromObj(interp, canvas, objv[i+1], &y) != TCL_OK) {
                return TCL_ERROR;
            }
            if (i == 0) {
                firstX = x;
                firstY = y;
                atomPtr = NewMoveToAtom(x, y);
                firstAtomPtr = atomPtr;
            } else {
                atomPtr->nextPtr = NewLineToAtom(x, y);
                atomPtr = atomPtr->nextPtr;
            }
        }
        if (closed) {
            atomPtr->nextPtr = NewCloseAtom(firstX, firstY);
        }
        *atomPtrPtr = firstAtomPtr;
        *lenPtr = i/2 + 2;
    }
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
