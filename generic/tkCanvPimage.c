/*
 * tkCanvPimage.c --
 *
 *	This file implements an image canvas item modelled after its
 *	SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2007-2008  Mats Bengtsson
 *
 */

#include "tkIntPath.h"
#include "tkpCanvas.h"
#include "tkCanvPathUtil.h"
#include "tkPathStyle.h"

#define BBOX_OUT 2.0

MODULE_SCOPE int gCanZlib;

#define ZLIB_SUPPORT						\
    ((TCL_MAJOR_VERSION > 8) ||					\
     ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 6)))

/*
 * The structure below defines the record for each path item.
 */

typedef struct PimageItem  {
    Tk_PathItemEx headerEx; /* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_PathCanvas canvas;   /* Canvas containing item. */
    double fillOpacity;
    TMatrix *matrixPtr;	    /*  a  b   default (NULL): 1 0
				c  d		   0 1
				tx ty 		   0 0 */
    double coord[2];	    /* nw coord. */
    Tcl_Obj *imageObj;	    /* Object describing the -image option.
			     * NULL means no image right now. */
    Tk_Image image;	    /* Image to display in window, or NULL if
                             * no image at present. */
    Tk_PhotoHandle photo;
    double width;	    /* If 0 use natural width or height. */
    double height;
    int anchor;
    XColor *tintColor;
    double tintAmount;
    int interpolation;
    PathRect *srcRegionPtr;
} PimageItem;

/*
 * Prototypes for procedures defined in this file:
 */

static void	ComputePimageBbox(Tk_PathCanvas canvas, PimageItem *pimagePtr);
static int	ConfigurePimage(Tcl_Interp *interp, Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, Tcl_Size objc,
		    Tcl_Obj *const objv[], int flags);
static int	CreatePimage(Tcl_Interp *interp,
		    Tk_PathCanvas canvas, struct Tk_PathItem *itemPtr,
		    Tcl_Size objc, Tcl_Obj *const objv[]);
static void	DeletePimage(Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, Display *display);
static void	DisplayPimage(Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, Display *display, Drawable drawable,
		    int x, int y, int width, int height);
static void	PimageBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
		    int mask);
static int	PimageCoords(Tcl_Interp *interp,
		    Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
		    Tcl_Size objc, Tcl_Obj *const objv[]);
static int	PimageToArea(Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, double *rectPtr);
static int	PimageToPdf(Tcl_Interp *interp, Tk_PathCanvas canvas,
		    Tk_PathItem *item, Tcl_Size objc, Tcl_Obj *const objv[],
		    int prepass);
static double	PimageToPoint(Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, double *coordPtr);
#ifndef TKP_NO_POSTSCRIPT
static int	PimageToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas,
		    Tk_PathItem *itemPtr, int prepass);
#endif
static void	ScalePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
		    int compensate, double originX, double originY,
		    double scaleX, double scaleY);
static void	TranslatePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
		    int compensate, double deltaX, double deltaY);
static void	ImageChangedProc(ClientData clientData,
		    int x, int y, int width, int height, int imgWidth,
		    int imgHeight);
static void	PimageStyleChangedProc(ClientData clientData, int flags);

enum {
    PIMAGE_OPTION_INDEX_FILLOPACITY =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 1)),
    PIMAGE_OPTION_INDEX_HEIGHT =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 2)),
    PIMAGE_OPTION_INDEX_IMAGE =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 3)),
    PIMAGE_OPTION_INDEX_MATRIX =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 4)),
    PIMAGE_OPTION_INDEX_WIDTH =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 5)),
    PIMAGE_OPTION_INDEX_ANCHOR =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 6)),
    PIMAGE_OPTION_INDEX_TINTCOLOR =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 7)),
    PIMAGE_OPTION_INDEX_TINTAMOUNT =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 8)),
    PIMAGE_OPTION_INDEX_INTERPOLATION =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 9)),
    PIMAGE_OPTION_INDEX_SRCREGION =
	(1L << (PATH_STYLE_OPTION_INDEX_END + 10))
};

static const char *imageAnchorST[] = {
    "n", "w", "s", "e", "nw", "ne", "sw", "se", "c", NULL
};

static const char *imageInterpolationST[] = {
    "none", "fast", "best", NULL
};

static int	PathRectSetOption(ClientData clientData, Tcl_Interp *interp,
		    Tk_Window tkwin, Tcl_Obj **value, char *recordPtr,
		    Tcl_Size internalOffset, char *oldInternalPtr, int flags);
static Tcl_Obj *PathRectGetOption(ClientData clientData, Tk_Window tkwin,
		    char *recordPtr, Tcl_Size internalOffset);
static void	PathRectRestoreOption(ClientData clientData, Tk_Window tkwin,
		    char *internalPtr, char *oldInternalPtr);
static void	PathRectFreeOption(ClientData clientData, Tk_Window tkwin,
		    char *internalPtr);

#define PATH_STYLE_CUSTOM_OPTION_PATHRECT	\
    static Tk_ObjCustomOption pathRectCO = {	\
        "pathrect",				\
        PathRectSetOption,			\
        PathRectGetOption,			\
        PathRectRestoreOption,			\
        PathRectFreeOption,			\
        (ClientData) NULL			\
    };

PATH_STYLE_CUSTOM_OPTION_MATRIX
PATH_CUSTOM_OPTION_TAGS
PATH_OPTION_STRING_TABLES_STATE
PATH_STYLE_CUSTOM_OPTION_PATHRECT

#define PATH_OPTION_SPEC_FILLOPACITY			    \
    {TK_OPTION_DOUBLE, "-fillopacity", NULL, NULL,	    \
        "1.0", -1, offsetof(PimageItem, fillOpacity),	    \
	0, 0, PIMAGE_OPTION_INDEX_FILLOPACITY}

#define PATH_OPTION_SPEC_HEIGHT				    \
    {TK_OPTION_DOUBLE, "-height", NULL, NULL,		    \
        "0", -1, offsetof(PimageItem, height),		    \
	0, 0, PIMAGE_OPTION_INDEX_HEIGHT}

#define PATH_OPTION_SPEC_IMAGE				    \
    {TK_OPTION_STRING, "-image", NULL, NULL,		    \
        NULL, offsetof(PimageItem, imageObj), -1,	    \
	TK_OPTION_NULL_OK, 0, PIMAGE_OPTION_INDEX_IMAGE}

#define PATH_OPTION_SPEC_MATRIX				    \
    {TK_OPTION_CUSTOM, "-matrix", NULL, NULL,		    \
	NULL, -1, offsetof(PimageItem, matrixPtr),	    \
	TK_OPTION_NULL_OK, (ClientData) &matrixCO,	    \
	PIMAGE_OPTION_INDEX_MATRIX}

#define PATH_OPTION_SPEC_WIDTH				    \
    {TK_OPTION_DOUBLE, "-width", NULL, NULL,		    \
        "0", -1, offsetof(PimageItem, width),		    \
        0, 0, PIMAGE_OPTION_INDEX_WIDTH}

#define PATH_OPTION_SPEC_ANCHOR				    \
    {TK_OPTION_STRING_TABLE, "-anchor", NULL, NULL,	    \
	"nw", -1, offsetof(PimageItem, anchor),	    \
        0, (ClientData) imageAnchorST, 0}

#define PATH_OPTION_SPEC_TINTCOLOR			    \
    {TK_OPTION_COLOR, "-tintcolor", NULL, NULL,		    \
        NULL, -1, offsetof(PimageItem, tintColor),	    \
        TK_OPTION_NULL_OK, 0, PIMAGE_OPTION_INDEX_TINTCOLOR}

#define PATH_OPTION_SPEC_TINTAMOUNT			    \
    {TK_OPTION_DOUBLE, "-tintamount", NULL, NULL,	    \
	"0.5", -1, offsetof(PimageItem, tintAmount),	    \
	0, 0, PIMAGE_OPTION_INDEX_TINTAMOUNT}

#define PATH_OPTION_SPEC_INTERPOLATION			    \
    {TK_OPTION_STRING_TABLE, "-interpolation", NULL, NULL,  \
        "fast", -1, offsetof(PimageItem, interpolation),   \
        0, (ClientData) imageInterpolationST, 0}

#define PATH_OPTION_SPEC_SRCREGION			    \
    {TK_OPTION_CUSTOM, "-srcregion", NULL, NULL,	    \
	NULL, -1, offsetof(PimageItem, srcRegionPtr),	    \
	TK_OPTION_NULL_OK, (ClientData) &pathRectCO,	    \
	PIMAGE_OPTION_INDEX_SRCREGION}

static Tk_OptionSpec optionSpecs[] = {
    PATH_OPTION_SPEC_CORE(Tk_PathItemEx),
    PATH_OPTION_SPEC_PARENT,
    PATH_OPTION_SPEC_MATRIX,
    PATH_OPTION_SPEC_FILLOPACITY,
    PATH_OPTION_SPEC_HEIGHT,
    PATH_OPTION_SPEC_IMAGE,
    PATH_OPTION_SPEC_WIDTH,
    PATH_OPTION_SPEC_ANCHOR,
    PATH_OPTION_SPEC_TINTCOLOR,
    PATH_OPTION_SPEC_TINTAMOUNT,
    PATH_OPTION_SPEC_INTERPOLATION,
    PATH_OPTION_SPEC_SRCREGION,
    PATH_OPTION_SPEC_END
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_PathItemType tkPimageType = {
    "pimage",				/* name */
    sizeof(PimageItem),			/* itemSize */
    CreatePimage,			/* createProc */
    optionSpecs,			/* optionSpecs */
    ConfigurePimage,			/* configureProc */
    PimageCoords,			/* coordProc */
    DeletePimage,			/* deleteProc */
    DisplayPimage,			/* displayProc */
    0,					/* flags */
    PimageBbox,				/* bboxProc */
    PimageToPoint,			/* pointProc */
    PimageToArea,			/* areaProc */
#ifndef TKP_NO_POSTSCRIPT
    PimageToPostscript,			/* postscriptProc */
#endif
    PimageToPdf,			/* pdfProc */
    ScalePimage,			/* scaleProc */
    TranslatePimage,			/* translateProc */
    (Tk_PathItemIndexProc *) NULL,	/* indexProc */
    (Tk_PathItemCursorProc *) NULL,	/* icursorProc */
    (Tk_PathItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_PathItemInsertProc *) NULL,	/* insertProc */
    (Tk_PathItemDCharsProc *) NULL,	/* dTextProc */
    (Tk_PathItemType *) NULL,		/* nextPtr */
};

static int
CreatePimage(Tcl_Interp *interp, Tk_PathCanvas canvas,
    Tk_PathItem *itemPtr, Tcl_Size objc, Tcl_Obj *const objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathItemEx *itemExPtr = &pimagePtr->headerEx;
    Tcl_Size i;
    Tk_OptionTable optionTable;

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
    pimagePtr->fillOpacity = 1.0;
    pimagePtr->matrixPtr = NULL;
    pimagePtr->imageObj = NULL;
    pimagePtr->image = NULL;
    pimagePtr->photo = NULL;
    pimagePtr->height = 0;
    pimagePtr->width = 0;
    pimagePtr->anchor = kPathImageAnchorNW;
    pimagePtr->tintColor = NULL;
    pimagePtr->tintAmount = 0.0;
    pimagePtr->interpolation = kPathImageInterpolationFast;
    pimagePtr->srcRegionPtr = NULL;
    itemPtr->bbox = NewEmptyPathRect();

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);
    itemPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, (char *) pimagePtr, optionTable,
	    Tk_PathCanvasTkwin(canvas)) != TCL_OK) {
        goto error;
    }

    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }
    if (CoordsForPointItems(interp, canvas, pimagePtr->coord, i, objv)
	!= TCL_OK) {
        goto error;
    }
    if (ConfigurePimage(interp, canvas, itemPtr, objc-i, objv+i, 0)
	== TCL_OK) {
        return TCL_OK;
    }

    error:
    /*
     * NB: We must unlink the item here since the ConfigurePimage()
     *     link it to the root by default.
     */
    TkPathCanvasItemDetach(itemPtr);
    DeletePimage(canvas, itemPtr, Tk_Display(Tk_PathCanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int
PimageCoords(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    int result;

    result = CoordsForPointItems(interp, canvas, pimagePtr->coord, objc, objv);
    if ((result == TCL_OK) && ((objc == 1) || (objc == 2))) {
        ComputePimageBbox(canvas, pimagePtr);
    }
    return result;
}

/*
 * This is just a convenience function to obtain any style matrix.
 */

static TMatrix
GetTMatrix(PimageItem *pimagePtr)
{
    TMatrix *matrixPtr;
    Tk_PathStyle *stylePtr;
    TMatrix matrix = TkPathCanvasInheritTMatrix((Tk_PathItem *) pimagePtr);

    matrixPtr = pimagePtr->matrixPtr;
    if (pimagePtr->headerEx.styleInst != NULL) {
	stylePtr = pimagePtr->headerEx.styleInst->masterPtr;
	if (stylePtr->mask & PATH_STYLE_OPTION_MATRIX) {
	    matrixPtr = stylePtr->matrixPtr;
	}
    }
    if (matrixPtr != NULL) {
	MMulTMatrix(matrixPtr, &matrix);
    }
    return matrix;
}

void
ComputePimageBbox(Tk_PathCanvas canvas, PimageItem *pimagePtr)
{
    Tk_PathItem *itemPtr = (Tk_PathItem *)pimagePtr;
    Tk_PathState state = pimagePtr->headerEx.header.state;
    TMatrix matrix;
    double width = 0.0, height = 0.0;
    PathRect bbox;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if (pimagePtr->image == NULL) {
        pimagePtr->headerEx.header.x1 = pimagePtr->headerEx.header.x2 =
        pimagePtr->headerEx.header.y1 = pimagePtr->headerEx.header.y2 = -1;
        return;
    }
    if (pimagePtr->srcRegionPtr) {
        width  = pimagePtr->srcRegionPtr->x2 - pimagePtr->srcRegionPtr->x1;
        height = pimagePtr->srcRegionPtr->y2 - pimagePtr->srcRegionPtr->y1;
    } else {
        int iwidth = 0, iheight = 0;
        Tk_SizeOfImage(pimagePtr->image, &iwidth, &iheight);
        width = iwidth;
        height = iheight;
    }
    if (pimagePtr->width > 0.0) {
        width = pimagePtr->width + 1.0;
    }
    if (pimagePtr->height > 0.0) {
        height = pimagePtr->height + 1.0;
    }

    switch (pimagePtr->anchor) {
        case kPathImageAnchorW:
        case kPathImageAnchorNW:
        case kPathImageAnchorSW:
            bbox.x1 = pimagePtr->coord[0];
            bbox.x2 = bbox.x1 + width;
            break;
        case kPathImageAnchorN:
        case kPathImageAnchorS:
        case kPathImageAnchorC:
            bbox.x1 = pimagePtr->coord[0] - width/2.0;
            bbox.x2 = pimagePtr->coord[0] + width/2.0;
            break;
        case kPathImageAnchorE:
        case kPathImageAnchorNE:
        case kPathImageAnchorSE:
            bbox.x1 = pimagePtr->coord[0] - width;
            bbox.x2 = pimagePtr->coord[0];
            break;
        default:
            break;
    }

    switch (pimagePtr->anchor) {
        case kPathImageAnchorN:
        case kPathImageAnchorNW:
        case kPathImageAnchorNE:
            bbox.y1 = pimagePtr->coord[1];
            bbox.y2 = pimagePtr->coord[1] + height;
            break;
        case kPathImageAnchorW:
        case kPathImageAnchorE:
        case kPathImageAnchorC:
            bbox.y1 = pimagePtr->coord[1] - height/2.0;
            bbox.y2 = pimagePtr->coord[1] + height/2.0;
            break;
        case kPathImageAnchorS:
        case kPathImageAnchorSW:
        case kPathImageAnchorSE:
            bbox.y1 = pimagePtr->coord[1] - height;
            bbox.y2 = pimagePtr->coord[1];
            break;
        default:
            break;
    }
    bbox.x1 -= BBOX_OUT;
    bbox.x2 += BBOX_OUT;
    bbox.y1 -= BBOX_OUT;
    bbox.y2 += BBOX_OUT;

    itemPtr->bbox = bbox;
    itemPtr->totalBbox = itemPtr->bbox;    /* FIXME */
    matrix = GetTMatrix(pimagePtr);
    SetGenericPathHeaderBbox(&pimagePtr->headerEx.header, &matrix, &bbox);
}

static int
ConfigurePimage(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], int flags)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_Window tkwin;
    Tk_Image image;
    Tk_PhotoHandle photo;
    Tk_SavedOptions savedOptions;
    Tk_PathItem *parentPtr;
    Tcl_Obj *errorResult = NULL;
    int error, mask;

    tkwin = Tk_PathCanvasTkwin(canvas);
    for (error = 0; error <= 1; error++) {
	if (!error) {
	    if (Tk_SetOptions(interp, (char *) pimagePtr, itemPtr->optionTable,
		    objc, objv, tkwin, &savedOptions, &mask) != TCL_OK) {
		continue;
	    }
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);
	}

	/*
	 * Take each custom option, not handled in Tk_SetOptions, in turn.
	 */
	if (mask & PATH_CORE_OPTION_PARENT) {
	    if (TkPathCanvasFindGroup(interp, canvas, itemPtr->parentObj,
				      &parentPtr) != TCL_OK) {
		continue;
	    }
	    TkPathCanvasSetParent(parentPtr, itemPtr);
	} else if ((itemPtr->id != 0) && (itemPtr->parentPtr == NULL)) {
	    /*
	     * If item not root and parent not set we must set it
	     * to root by default.
	     */
	    CanvasSetParentToRoot(itemPtr);
	}

	/*
	 * If we have got a style name it's options take precedence
	 * over the actual path configuration options. This is how SVG does it.
	 * Good or bad?
	 */
	if (mask & PATH_CORE_OPTION_STYLENAME) {
	    TkPathStyleInst *styleInst = NULL;

	    if (pimagePtr->headerEx.styleObj != NULL) {
		styleInst = TkPathGetStyle(interp,
			Tcl_GetString(pimagePtr->headerEx.styleObj),
			TkPathCanvasStyleTable(canvas), PimageStyleChangedProc,
			(ClientData) itemPtr);
		if (styleInst == NULL) {
		    continue;
		}
	    } else {
		styleInst = NULL;
	    }
	    if (pimagePtr->headerEx.styleInst != NULL) {
		TkPathFreeStyle(pimagePtr->headerEx.styleInst);
	    }
	    pimagePtr->headerEx.styleInst = styleInst;
	}

	/*
	 * Create the image.  Save the old image around and don't free it
	 * until after the new one is allocated.  This keeps the reference
	 * count from going to zero so the image doesn't have to be recreated
	 * if it hasn't changed.
	 */
	if (mask & PIMAGE_OPTION_INDEX_IMAGE) {
	    if (pimagePtr->imageObj != NULL) {
		image = Tk_GetImage(interp, tkwin,
			Tcl_GetString(pimagePtr->imageObj),
			ImageChangedProc, (ClientData) pimagePtr);
		if (image == NULL) {
		    continue;
		}
		photo =
		    Tk_FindPhoto(interp, Tcl_GetString(pimagePtr->imageObj));
		if (photo == NULL) {
		    continue;
		}
	    } else {
		image = NULL;
		photo = NULL;
	    }
	    if (pimagePtr->image != NULL) {
		Tk_FreeImage(pimagePtr->image);
	    }
	    pimagePtr->image = image;
	    pimagePtr->photo = photo;
	}

	/*
	 * If we reach this on the first pass we are OK and continue below.
	 */
	break;
    }
    if (!error) {
	Tk_FreeSavedOptions(&savedOptions);
    }
    pimagePtr->fillOpacity = MAX(0.0, MIN(1.0, pimagePtr->fillOpacity));

    /*
     * Recompute bounding box for path.
     */
    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    } else {
	ComputePimageBbox(canvas, pimagePtr);
	return TCL_OK;
    }
}

static void
DeletePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    if (pimagePtr->headerEx.styleInst != NULL) {
	TkPathFreeStyle(pimagePtr->headerEx.styleInst);
    }
    if (pimagePtr->image != NULL) {
        Tk_FreeImage(pimagePtr->image);
    }
    Tk_FreeConfigOptions((char *) pimagePtr, itemPtr->optionTable,
			 Tk_PathCanvasTkwin(canvas));
}

static void
DisplayPimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, Display *display,
    Drawable drawable,
    int x, int y, int width, int height)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    TkPathContext ctx;

    ctx = TkPathInit(Tk_PathCanvasTkwin(canvas), drawable);
    TkPathPushTMatrix(ctx, &m);
    m = GetTMatrix(pimagePtr);
    TkPathPushTMatrix(ctx, &m);
    /* @@@ Maybe we should taking care of x, y etc.? */
    TkPathImage(ctx, pimagePtr->image, pimagePtr->photo,
            itemPtr->bbox.x1+BBOX_OUT, itemPtr->bbox.y1+BBOX_OUT,
            pimagePtr->width, pimagePtr->height, pimagePtr->fillOpacity,
            pimagePtr->tintColor, pimagePtr->tintAmount,
	    pimagePtr->interpolation,
            pimagePtr->srcRegionPtr);
    TkPathFree(ctx);
}

static void
PimageBbox(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int mask)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    ComputePimageBbox(canvas, pimagePtr);
}

static double
PimageToPoint(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *pointPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    TMatrix m = GetTMatrix(pimagePtr);
    return PathRectToPointWithMatrix(itemPtr->bbox, &m, pointPtr);
}

static int
PimageToArea(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, double *areaPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    TMatrix m = GetTMatrix(pimagePtr);
    return PathRectToAreaWithMatrix(itemPtr->bbox, &m, areaPtr);
}

static int
PimageToPdf(Tcl_Interp *interp, Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    Tcl_Size objc, Tcl_Obj *const objv[], int prepass)
{
    Tk_PathStyle style;
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tcl_Obj *ret, *obj;
    TMatrix matrix = { 1., 0., 0., 1., 0., 0.};
    Tk_PathState state = itemPtr->state;
    Tk_PhotoImageBlock block;
    int x, y;
    unsigned char *p;
    double tintR, tintG, tintB, tintAmount;
    PathRect srcRegion, bbox;

    if (state == TK_PATHSTATE_NULL) {
	state = TkPathCanvasState(canvas);
    }
    if ((pimagePtr->photo == NULL) || (state == TK_PATHSTATE_HIDDEN)) {
	return TCL_OK;	/* nothing to display */
    }
    Tk_PhotoGetImage(pimagePtr->photo, &block);
    if ((block.width <= 0) || (block.height <= 0)) {
	return TCL_OK;	/* nothing to display */
    }
    if (pimagePtr->srcRegionPtr == NULL) {
	srcRegion.x1 = 0;
	srcRegion.y1 = 0;
	srcRegion.x2 = block.width;
	srcRegion.y2 = block.height;
    } else {
	srcRegion = *pimagePtr->srcRegionPtr;
    }
    bbox = itemPtr->bbox;
    /* undo effect of BBOX_OUT */
    bbox.x1 += BBOX_OUT - 1;
    bbox.x2 -= BBOX_OUT + 1;
    bbox.y1 += BBOX_OUT - 1;
    bbox.y2 -= BBOX_OUT + 1;

    if (objc > 0) {
	long id;
	Tcl_Size retc;
	int opacity, tx, ty;
#if ZLIB_SUPPORT
	Tcl_Size zLen;
#endif
	double scaleX, scaleY, dx, dy;
	Tcl_Obj *pixObj, *nameObj, **retv;

	/*
	 * Callback provided, first make alpha mask.
	 */
	opacity = pimagePtr->fillOpacity * 256;
	if (opacity > 256) {
	    opacity = 256;
	} else if (opacity < 0) {
	    opacity = 0;
	}
	pixObj = Tcl_NewObj();
	p = Tcl_SetByteArrayLength(pixObj, block.width * block.height);
	for (y = 0; y < block.height; y++) {
	    unsigned char *q = block.pixelPtr + y * block.pitch;

	    for (x = 0; x < block.width; x++) {
		*p++ = ((int) q[block.offset[3]] * opacity) >> 8;
		q += block.pixelSize;
	    }
	}
#if ZLIB_SUPPORT
	zLen = 0;
	if (gCanZlib) {
	    if (Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_ZLIB,
				pixObj, 9, NULL) == TCL_OK) {
		p = Tcl_GetByteArrayFromObj(Tcl_GetObjResult(interp), &zLen);
		if (zLen > 0) {
		    Tcl_DecrRefCount(pixObj);
		    pixObj = Tcl_GetObjResult(interp);
		}
		Tcl_IncrRefCount(pixObj);
	    }
	    Tcl_ResetResult(interp);
	} else {
	    Tcl_IncrRefCount(pixObj);
	}
#else
	Tcl_IncrRefCount(pixObj);
#endif
	obj = Tcl_NewObj();
	Tcl_AppendPrintfToObj(obj, "<<\n/Type /XObject\n/Subtype /Image\n"
			      "/ColorSpace /DeviceGray\n/BitsPerComponent 8\n"
			      "/Width %d\n/Height %d\n",
			      block.width, block.height);
#if ZLIB_SUPPORT
	if (zLen > 0) {
	    Tcl_AppendPrintfToObj(obj, "/Filter /FlateDecode\n"
				  "/Length %ld\n>>\nstream\n", zLen);
	} else
#endif
	{
	    Tcl_AppendPrintfToObj(obj, "/Length %d\n>>\nstream\n",
				  block.width * block.height);
	}
	Tcl_AppendObjToObj(obj, pixObj);
	Tcl_DecrRefCount(pixObj);
	Tcl_AppendToObj(obj, "\nendstream\n", 11);
	ret = Tcl_DuplicateObj(objv[0]);
	Tcl_IncrRefCount(ret);
	if ((Tcl_ListObjAppendElement(interp, ret,
				      Tcl_NewIntObj(block.width)) != TCL_OK) ||
	    (Tcl_ListObjAppendElement(interp, ret,
				      Tcl_NewIntObj(block.height)) != TCL_OK) ||
	    (Tcl_ListObjAppendElement(interp, ret, obj) != TCL_OK)) {
	    Tcl_DecrRefCount(ret);
	    Tcl_DecrRefCount(obj);
	    return TCL_ERROR;
	}
	if (Tcl_EvalObjEx(interp, ret, TCL_EVAL_DIRECT) != TCL_OK) {
	    Tcl_DecrRefCount(ret);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(ret);
	/*
	 * Remember result information (object id of alpha channel).
	 */
	if (Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp),
				   &retc, &retv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (retc < 1) {
	    Tcl_SetResult(interp, (char *) "missing PDF id", TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_GetLongFromObj(NULL, retv[0], &id);
	/*
	 * Make RGB image.
	 */
	tintAmount = pimagePtr->tintAmount;
	if ((pimagePtr->tintColor != NULL) && (tintAmount > 0.0)) {
	    if (tintAmount > 1.0) {
		tintAmount = 1.0;
	    }
	    tintR = (double) (pimagePtr->tintColor->red >> 8) / 0xff;
	    tintG = (double) (pimagePtr->tintColor->green >> 8) / 0xff;
	    tintB = (double) (pimagePtr->tintColor->blue >> 8) / 0xff;
	} else {
	    tintAmount = 0.0;
	    tintR = 0.0;
	    tintG = 0.0;
	    tintB = 0.0;
	}
	pixObj = Tcl_NewObj();
	p = Tcl_SetByteArrayLength(pixObj, block.width * block.height * 3);
	for (y = 0; y < block.height; y++) {
	    unsigned char R, G, B;
	    unsigned char *q = block.pixelPtr + y * block.pitch;

	    for (x = 0; x < block.width; x++) {
		R = q[block.offset[0]];
		G = q[block.offset[1]];
		B = q[block.offset[2]];
		if (tintAmount > 0.0) {
		    int RR, GG, BB;

		    RR = (1.0 - tintAmount) * R +
			(tintAmount * tintR * 0.2126 * R +
			 tintAmount * tintR * 0.7152 * G +
			 tintAmount * tintR * 0.0722 * B);
		    GG = (1.0 - tintAmount) * G +
			(tintAmount * tintG * 0.2126 * R +
			 tintAmount * tintG * 0.7152 * G +
			 tintAmount * tintG * 0.0722 * B);
		    BB = (1.0 - tintAmount) * B +
			(tintAmount * tintB * 0.2126 * R +
			 tintAmount * tintB * 0.7152 * G +
			 tintAmount * tintB * 0.0722 * B);
		    R = (RR > 0xff) ? 0xff : RR;
		    G = (GG > 0xff) ? 0xff : GG;
		    B = (BB > 0xff) ? 0xff : BB;
		}
		*p++ = R;
		*p++ = G;
		*p++ = B;
		q += block.pixelSize;
	    }
	}
#if ZLIB_SUPPORT
	zLen = 0;
	if (gCanZlib) {
	    if (Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_ZLIB,
				pixObj, 9, NULL) == TCL_OK) {
		p = Tcl_GetByteArrayFromObj(Tcl_GetObjResult(interp), &zLen);
		if (zLen > 0) {
		    Tcl_DecrRefCount(pixObj);
		    pixObj = Tcl_GetObjResult(interp);
		}
		Tcl_IncrRefCount(pixObj);
	    }
	    Tcl_ResetResult(interp);
	} else {
	    Tcl_IncrRefCount(pixObj);
	}
#else
	Tcl_IncrRefCount(pixObj);
#endif
	obj = Tcl_NewObj();
	Tcl_AppendPrintfToObj(obj, "<<\n/Type /XObject\n/Subtype /Image\n"
			      "/ColorSpace /DeviceRGB\n/BitsPerComponent 8\n"
			      "/Width %d\n/Height %d\n/SMask %ld 0 R\n",
			      block.width, block.height, id);
	if ((pimagePtr->interpolation == kPathImageInterpolationFast) ||
	    (pimagePtr->interpolation == kPathImageInterpolationBest)) {
	    Tcl_AppendToObj(obj, "/Interpolate true\n", 18);
	}
#if ZLIB_SUPPORT
	if (zLen > 0) {
	    Tcl_AppendPrintfToObj(obj, "/Filter /FlateDecode\n"
				  "/Length %ld\n>>\nstream\n", zLen);
	} else
#endif
	{
	    Tcl_AppendPrintfToObj(obj, "/Length %d\n>>\nstream\n",
				  block.width * block.height * 3);
	}
	Tcl_AppendObjToObj(obj, pixObj);
	Tcl_DecrRefCount(pixObj);
	Tcl_AppendToObj(obj, "\nendstream\n", 11);
	ret = Tcl_DuplicateObj(objv[0]);
	Tcl_IncrRefCount(ret);
	if ((Tcl_ListObjAppendElement(interp, ret,
				      Tcl_NewIntObj(block.width)) != TCL_OK) ||
	    (Tcl_ListObjAppendElement(interp, ret,
				      Tcl_NewIntObj(block.height)) != TCL_OK) ||
	    (Tcl_ListObjAppendElement(interp, ret, obj) != TCL_OK)) {
	    Tcl_DecrRefCount(ret);
	    Tcl_DecrRefCount(obj);
	    return TCL_ERROR;
	}
	if (Tcl_EvalObjEx(interp, ret, TCL_EVAL_DIRECT) != TCL_OK) {
	    Tcl_DecrRefCount(ret);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(ret);
	/*
	 * Remember result information (name of RGB image).
	 */
	if (Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp),
				   &retc, &retv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (retc < 2) {
	    Tcl_SetResult(interp, (char *) "missing PDF id/name", TCL_STATIC);
	    return TCL_ERROR;
	}
	nameObj = retv[1];
	Tcl_IncrRefCount(nameObj);
	/*
	 * Print image object(s).
	 */
	ret = Tcl_NewObj();
	style = TkPathCanvasInheritStyle(itemPtr, 0);
	matrix = GetTMatrix(pimagePtr);
	/* TMatrix */
	TkPathPdfNumber(ret, 6, matrix.a, " ");
	TkPathPdfNumber(ret, 6, matrix.b, " ");
	TkPathPdfNumber(ret, 6, matrix.c, " ");
	TkPathPdfNumber(ret, 6, matrix.d, " ");
	TkPathPdfNumber(ret, 3, matrix.tx, " ");
	TkPathPdfNumber(ret, 3, matrix.ty, " cm\n");
	/* translate */
	Tcl_AppendToObj(ret, "1 0 0 1 ", 8);
	TkPathPdfNumber(ret, 3, bbox.x1, " ");
	TkPathPdfNumber(ret, 3, bbox.y2, " cm\n");
	/* clipping */
	Tcl_AppendToObj(ret, "0 0 m ", 6);
	TkPathPdfNumber(ret, 3, bbox.x2 - bbox.x1, " 0 l ");
	TkPathPdfNumber(ret, 3, bbox.x2 - bbox.x1, " ");
	TkPathPdfNumber(ret, 3, bbox.y1 - bbox.y2, " l 0 ");
	TkPathPdfNumber(ret, 3, bbox.y1 - bbox.y2, " l W n\n");
	tx = ty = 1;
	dx = srcRegion.x2 - srcRegion.x1;
	if (dx > block.width) {
	    tx += (srcRegion.x2 - srcRegion.x1) / block.width;
	} else {
	    srcRegion.x1 = fmod(srcRegion.x1, (double) block.width);
	    srcRegion.x2 = srcRegion.x1 + dx;
	    if (srcRegion.x1 != 0) {
		tx++;
	    }
	}
	dy = srcRegion.y2 - srcRegion.y1;
	if (dy > block.height) {
	    ty += (srcRegion.y2 - srcRegion.y1) / block.height;
	} else {
	    srcRegion.y1 = fmod(srcRegion.y1, (double) block.height);
	    srcRegion.y2 = srcRegion.y1 + dy;
	    if (srcRegion.y1 != 0) {
		ty++;
	    }
	}
	if (pimagePtr->width > 0) {
	    scaleX = pimagePtr->width / (double) block.width;
	    scaleX /= (srcRegion.x2 - srcRegion.x1) / block.width;
	} else {
	    scaleX = 1.0;
	}
	if (pimagePtr->height > 0) {
	    scaleY = pimagePtr->height / (double) block.height;
	    scaleY /= (srcRegion.y2 - srcRegion.y1) / block.height;
	} else {
	    scaleY = 1.0;
	}
	for (x = 0; x < tx; x++) {
	    for (y = 0; y < ty; y++) {
		/* translate */
		Tcl_AppendToObj(ret, "q\n1 0 0 1 ", 10);
		TkPathPdfNumber(ret, 6,	(x * block.width - srcRegion.x1) *
				scaleX, " ");
		TkPathPdfNumber(ret, 6,	-(srcRegion.y2 - (ty - y) *
					  block.height) * scaleY, " cm\n");
		/* scale to requested image size */
		TkPathPdfNumber(ret, 6,	scaleX * block.width, " 0 0 ");
		TkPathPdfNumber(ret, 6,	-scaleY * block.height, " 0 0 cm\n");
		/* print it */
		Tcl_AppendPrintfToObj(ret, "/%s Do\nQ\n",
			Tcl_GetString(nameObj));
	    }
	}
	Tcl_DecrRefCount(nameObj);
	Tcl_SetObjResult(interp, ret);
	TkPathCanvasFreeInheritedStyle(&style);
    } else {
	/*
	 * No callback, make direct RGB image without alpha mask.
	 */
	ret = Tcl_NewObj();
	style = TkPathCanvasInheritStyle(itemPtr, 0);
	matrix = GetTMatrix(pimagePtr);
	/* TMatrix */
	TkPathPdfNumber(ret, 6, matrix.a, " ");
	TkPathPdfNumber(ret, 6, matrix.b, " ");
	TkPathPdfNumber(ret, 6, matrix.c, " ");
	TkPathPdfNumber(ret, 6, matrix.d, " ");
	TkPathPdfNumber(ret, 3, matrix.tx, " ");
	TkPathPdfNumber(ret, 3, matrix.ty, " cm\n");
	/* translate */
	Tcl_AppendToObj(ret, "1 0 0 1 ", 8);
	TkPathPdfNumber(ret, 3, bbox.x1, " ");
	TkPathPdfNumber(ret, 3, bbox.y2, " cm\n");
	/* clipping */
	Tcl_AppendToObj(ret, "0 0 m ", 6);
	TkPathPdfNumber(ret, 3, bbox.x2 - bbox.x1, " 0 l ");
	TkPathPdfNumber(ret, 3, bbox.x2 - bbox.x1, " ");
	TkPathPdfNumber(ret, 3, bbox.y1 - bbox.y2, " l 0 ");
	TkPathPdfNumber(ret, 3, bbox.y1 - bbox.y2, " l W n\n");
	/* scale to requested image size, no cropping/tiling */
	TkPathPdfNumber(ret, 6,	(pimagePtr->width > 0) ? pimagePtr->width :
			(double) block.width, " 0 0 ");
	TkPathPdfNumber(ret, 6,	(pimagePtr->height > 0) ? pimagePtr->height :
			(double) -block.height, " 0 0 cm\n");
	Tcl_AppendToObj(ret, "BI\n", 3);
	Tcl_AppendPrintfToObj(ret, "/W %d\n/H %d\n", block.width, block.height);
	Tcl_AppendToObj(ret, "/CS /RGB\n/BPC 8\nID\n", 19);
	tintAmount = pimagePtr->tintAmount;
	if ((pimagePtr->tintColor != NULL) && (tintAmount > 0.0)) {
	    if (tintAmount > 1.0) {
		tintAmount = 1.0;
	    }
	    tintR = (double) (pimagePtr->tintColor->red >> 8) / 0xff;
	    tintG = (double) (pimagePtr->tintColor->green >> 8) / 0xff;
	    tintB = (double) (pimagePtr->tintColor->blue >> 8) / 0xff;
	} else {
	    tintAmount = 0.0;
	    tintR = 0.0;
	    tintG = 0.0;
	    tintB = 0.0;
	}
	obj = Tcl_NewObj();
	p = Tcl_SetByteArrayLength(obj, block.width * block.height * 3);
	for (y = 0; y < block.height; y++) {
	    unsigned char R, G, B;
	    unsigned char *q = block.pixelPtr + y * block.pitch;

	    for (x = 0; x < block.width; x++) {
		R = q[block.offset[0]];
		G = q[block.offset[1]];
		B = q[block.offset[2]];
		if (tintAmount > 0.0) {
		    int RR, GG, BB;

		    RR = (1.0 - tintAmount) * R +
			(tintAmount * tintR * 0.2126 * R +
			 tintAmount * tintR * 0.7152 * G +
			 tintAmount * tintR * 0.0722 * B);
		    GG = (1.0 - tintAmount) * G +
			(tintAmount * tintG * 0.2126 * R +
			 tintAmount * tintG * 0.7152 * G +
			 tintAmount * tintG * 0.0722 * B);
		    BB = (1.0 - tintAmount) * B +
			(tintAmount * tintB * 0.2126 * R +
			 tintAmount * tintB * 0.7152 * G +
			 tintAmount * tintB * 0.0722 * B);
		    R = (RR > 0xff) ? 0xff : RR;
		    G = (GG > 0xff) ? 0xff : GG;
		    B = (BB > 0xff) ? 0xff : BB;
		}
		*p++ = R;
		*p++ = G;
		*p++ = B;
		q += block.pixelSize;
	    }
	}
	Tcl_AppendObjToObj(ret, obj);
	Tcl_DecrRefCount(obj);
	Tcl_AppendToObj(ret, "\nEI\n", 4);
	Tcl_SetObjResult(interp, ret);
	TkPathCanvasFreeInheritedStyle(&style);
    }
    return TCL_OK;
}

#ifndef TKP_NO_POSTSCRIPT
static int
PimageToPostscript(Tcl_Interp *interp, Tk_PathCanvas canvas,
    Tk_PathItem *itemPtr, int prepass)
{
    return TCL_ERROR;
}
#endif

static void
ScalePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr,
    int compensate, double originX, double originY,
    double scaleX, double scaleY)
{
    /* Skip? */
}

static void
TranslatePimage(Tk_PathCanvas canvas, Tk_PathItem *itemPtr, int compensate,
    double deltaX, double deltaY)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    CompensateTranslate(itemPtr, compensate, &deltaX, &deltaY);

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&itemPtr->bbox, deltaX, deltaY);
    pimagePtr->coord[0] += deltaX;
    pimagePtr->coord[1] += deltaY;
    TranslateItemHeader(itemPtr, deltaX, deltaY);
}

static void
ImageChangedProc(
    ClientData clientData,	/* Pointer to canvas item for image. */
    int x, int y,		/* Upper left pixel (within image)
                                 * that must be redisplayed. */
    int width, int height,	/* Dimensions of area to redisplay
                                 * (may be <= 0). */
    int imgWidth, int imgHeight)/* New dimensions of image. */
{
    PimageItem *pimagePtr = (PimageItem *) clientData;

    /*
     * If the image's size changed and it's not anchored at its
     * northwest corner then just redisplay the entire area of the
     * image.  This is a bit over-conservative, but we need to do
     * something because a size change also means a position change.
     */

    /* @@@ MUST consider our own width and height settings as well and TMatrix. */

    if (((pimagePtr->headerEx.header.x2 - pimagePtr->headerEx.header.x1) !=
	 imgWidth)
        || ((pimagePtr->headerEx.header.y2 - pimagePtr->headerEx.header.y1) !=
	    imgHeight)) {
        x = y = 0;
        width = imgWidth;
        height = imgHeight;
        Tk_PathCanvasEventuallyRedraw(pimagePtr->headerEx.canvas,
		pimagePtr->headerEx.header.x1, pimagePtr->headerEx.header.y1,
		pimagePtr->headerEx.header.x2, pimagePtr->headerEx.header.y2);
    }
    ComputePimageBbox(pimagePtr->headerEx.canvas, pimagePtr);
    Tk_PathCanvasEventuallyRedraw(pimagePtr->headerEx.canvas,
	    pimagePtr->headerEx.header.x1 + x,
	    pimagePtr->headerEx.header.y1 + y,
	    (int) (pimagePtr->headerEx.header.x1 + x + width),
            (int) (pimagePtr->headerEx.header.y1 + y + height));
}

static void
PimageStyleChangedProc(ClientData clientData, int flags)
{
    Tk_PathItem *itemPtr = (Tk_PathItem *) clientData;
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    if (flags) {
	if (flags & PATH_STYLE_FLAG_DELETE) {
	    TkPathFreeStyle(pimagePtr->headerEx.styleInst);
	    pimagePtr->headerEx.styleInst = NULL;
	    Tcl_DecrRefCount(pimagePtr->headerEx.styleObj);
	    pimagePtr->headerEx.styleObj = NULL;
	}
	Tk_PathCanvasEventuallyRedraw(pimagePtr->headerEx.canvas,
		itemPtr->x1, itemPtr->y1,
		itemPtr->x2, itemPtr->y2);
    }
}

static int
PathGetPathRect(
        Tcl_Interp* interp,
        const char *list,   /* Object containg the lists for the matrix. */
        PathRect *rectPtr) /* Where to store TMatrix corresponding
                                 * to list. Must be allocated! */
{
    const char **argv = NULL;
    Tcl_Size argc;
    int i;
    int result = TCL_OK;
    double tmp[4];

    /* Check matrix consistency. */
    if (Tcl_SplitList(interp, list, &argc, &argv) != TCL_OK) {
        result = TCL_ERROR;
        goto bail;
    }
    if (argc != 4) {
        Tcl_AppendResult(interp, "rect \"", list, "\" is inconsistent",
                (char *) NULL);
        result = TCL_ERROR;
        goto bail;
    }

    /* Take each row in turn. */
    for (i = 0; i < 4; i++) {
        if (Tcl_GetDouble(interp, argv[i], &(tmp[i])) != TCL_OK) {
            Tcl_AppendResult(interp, "rect \"", list, "\" is inconsistent", (char *) NULL);
            result = TCL_ERROR;
            goto bail;
        }
    }

    /* PathRect. */
    rectPtr->x1  = tmp[0];
    rectPtr->y1  = tmp[1];
    rectPtr->x2  = tmp[2];
    rectPtr->y2  = tmp[3];

bail:
    if (argv != NULL) {
        Tcl_Free((char *) argv);
    }
    return result;
}

static int
PathGetTclObjFromPathRect(
        Tcl_Interp* interp,
        PathRect *rectPtr,
        Tcl_Obj **listObjPtrPtr)
{
    Tcl_Obj     *listObj;

    /* @@@ Error handling remains. */

    listObj = Tcl_NewListObj( 0, (Tcl_Obj **) NULL );
    if (rectPtr != NULL) {
        Tcl_ListObjAppendElement(interp, listObj, Tcl_NewDoubleObj(rectPtr->x1));
        Tcl_ListObjAppendElement(interp, listObj, Tcl_NewDoubleObj(rectPtr->y1));
        Tcl_ListObjAppendElement(interp, listObj, Tcl_NewDoubleObj(rectPtr->x2));
        Tcl_ListObjAppendElement(interp, listObj, Tcl_NewDoubleObj(rectPtr->y2));
    }
    *listObjPtrPtr = listObj;
    return TCL_OK;
}

/*
 * The -srcregion custom option.
 */

static int
PathRectSetOption(
    ClientData clientData,
    Tcl_Interp *interp,     /* Current interp; may be used for errors. */
    Tk_Window tkwin,        /* Window for which option is being set. */
    Tcl_Obj **value,        /* Pointer to the pointer to the value object.
                             * We use a pointer to the pointer because
                             * we may need to return a value (NULL). */
    char *recordPtr,        /* Pointer to storage for the widget record. */
    Tcl_Size internalOffset,/* Offset within *recordPtr at which the
                               internal value is to be stored. */
    char *oldInternalPtr,   /* Pointer to storage for the old value. */
    int flags)          /* Flags for the option, set Tk_SetOptions. */
{
    char *internalPtr;      /* Points to location in record where
                             * internal representation of value should
                             * be stored, or NULL. */
    char *list;
    Tcl_Size length;
    Tcl_Obj *valuePtr;
    PathRect *newPtr;

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
            newPtr = (PathRect *) ckalloc(sizeof(PathRect));
            if (PathGetPathRect(interp, list, newPtr) != TCL_OK) {
                ckfree((char *) newPtr);
                return TCL_ERROR;
            }
        } else {
            newPtr = NULL;
        }
        *((PathRect **) oldInternalPtr) = *((PathRect **) internalPtr);
        *((PathRect **) internalPtr) = newPtr;
    }
    return TCL_OK;
}

static Tcl_Obj *
PathRectGetOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,        /* Pointer to widget record. */
    Tcl_Size internalOffset)/* Offset within *recordPtr containing the
                             * value. */
{
    char    *internalPtr;
    PathRect     *pathRectPtr;
    Tcl_Obj     *listObj;

    /* @@@ An alternative to this could be to have an objOffset in option table. */
    internalPtr = recordPtr + internalOffset;
    pathRectPtr = *((PathRect **) internalPtr);
    PathGetTclObjFromPathRect(NULL, pathRectPtr, &listObj);
    return listObj;
}

static void
PathRectRestoreOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,      /* Pointer to storage for value. */
    char *oldInternalPtr)   /* Pointer to old value. */
{
    *(PathRect **)internalPtr = *(PathRect **)oldInternalPtr;
}

static void
PathRectFreeOption(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)      /* Pointer to storage for value. */
{
    if (*((char **) internalPtr) != NULL) {
        ckfree(*((char **) internalPtr));
        *((char **) internalPtr) = NULL;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
