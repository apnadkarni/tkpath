/*
 * tkCanvPathUtil.h --
 *
 *	Header for support functions common to many path canvas items.
 *
 * Copyright (c) 2007-2008  Mats Bengtsson
 *
 */

#ifndef INCLUDED_TKCANVPATHUTIL_H
#define INCLUDED_TKCANVPATHUTIL_H

#include "tkIntPath.h"

#ifdef __cplusplus
extern "C" {
#endif

MODULE_SCOPE int	CoordsForPointItems(Tcl_Interp *interp,
			    Tk_PathCanvas canvas,
			    double *pointPtr, Tcl_Size objc, Tcl_Obj *const objv[]);
MODULE_SCOPE int	CoordsForRectangularItems(Tcl_Interp *interp,
			    Tk_PathCanvas canvas,
			    PathRect *rectPtr, Tcl_Size objc, Tcl_Obj *const objv[]);
MODULE_SCOPE PathRect	GetGenericBarePathBbox(PathAtom *atomPtr);
MODULE_SCOPE PathRect	GetGenericPathTotalBboxFromBare(PathAtom *atomPtr,
			    Tk_PathStyle *stylePtr, PathRect *bboxPtr);
MODULE_SCOPE void	SetGenericPathHeaderBbox(Tk_PathItem *headerPtr,
			    TMatrix *mPtr, PathRect *totalBboxPtr);
MODULE_SCOPE TMatrix	GetCanvasTMatrix(Tk_PathCanvas canvas);
MODULE_SCOPE PathRect	NewEmptyPathRect(void);
MODULE_SCOPE int	IsPathRectEmpty(PathRect *r);
MODULE_SCOPE void	IncludePointInRect(PathRect *r, double x, double y);
MODULE_SCOPE double	GenericPathToPoint(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, Tk_PathStyle *stylePtr,
			    PathAtom *atomPtr, int maxNumSegments,
			    double *pointPtr);
MODULE_SCOPE int	GenericPathToArea(Tk_PathCanvas canvas,
			    Tk_PathItem *itemPtr, Tk_PathStyle *stylePtr,
			    PathAtom * atomPtr, int maxNumSegments,
			    double *areaPtr);
MODULE_SCOPE void	TranslatePathAtoms(PathAtom *atomPtr, double deltaX,
			    double deltaY);
MODULE_SCOPE void	ScalePathAtoms(PathAtom *atomPtr, double originX,
			    double originY, double scaleX, double scaleY);
MODULE_SCOPE void	TranslatePathRect(PathRect *r, double deltaX,
			    double deltaY);
MODULE_SCOPE void	ScalePathRect(PathRect *r, double originX,
			    double originY, double scaleX, double scaleY);
MODULE_SCOPE void	TranslateItemHeader(Tk_PathItem *itemPtr,
			    double deltaX, double deltaY);
MODULE_SCOPE void	ScaleItemHeader(Tk_PathItem *itemPtr, double originX,
			    double originY, double scaleX, double scaleY);
MODULE_SCOPE void	CompensateTranslate(Tk_PathItem *itemPtr, int compensate,
			    double *deltaX, double *deltaY);
MODULE_SCOPE void	CompensateScale(Tk_PathItem *itemPtr, int compensate,
			    double *originX, double *originY,
			    double *scaleX, double *scaleY);

/*
 * The canvas 'Area' and 'Point' functions.
 */

MODULE_SCOPE int	PathPolyLineToArea(double *polyPtr, int numPoints,
			    double *rectPtr);
MODULE_SCOPE double	PathThickPolygonToPoint(int joinStyle, int capStyle,
			    double width, int isclosed, double *polyPtr,
			    int numPoints, double *pointPtr);
MODULE_SCOPE double	PathPolygonToPointEx(double *polyPtr, int numPoints,
			    double *pointPtr, int *intersectionsPtr,
			    int *nonzerorulePtr);
MODULE_SCOPE double	PathRectToPoint(double rectPtr[], double width,
			    int filled, double pointPtr[]);
MODULE_SCOPE int	PathRectToArea(double rectPtr[], double width,
			    int filled, double *areaPtr);
MODULE_SCOPE int	PathRectToAreaWithMatrix(PathRect bbox, TMatrix *mPtr,
			    double *areaPtr);
MODULE_SCOPE double	PathRectToPointWithMatrix(PathRect bbox, TMatrix *mPtr,
			    double *pointPtr);
MODULE_SCOPE void	CurveSegments(double control[], int includeFirst,
			    int numSteps, double *coordPtr);

/*
 * New API option parsing.
 */

#define PATH_DEF_STATE "normal"

/* These MUST be kept in sync with Tk_PathState! */

#define PATH_OPTION_STRING_TABLES_STATE				    \
    static const char *stateStrings[] = {			    \
	"active", "disabled", "normal", "hidden", NULL		    \
    };

#define PATH_CUSTOM_OPTION_TAGS					    \
    static Tk_ObjCustomOption tagsCO = {			    \
        "tags",							    \
        Tk_PathCanvasTagsOptionSetProc,				    \
        Tk_PathCanvasTagsOptionGetProc,				    \
        Tk_PathCanvasTagsOptionRestoreProc,			    \
        Tk_PathCanvasTagsOptionFreeProc,			    \
        (ClientData) NULL					    \
    };

#define PATH_OPTION_SPEC_PARENT					    \
    {TK_OPTION_STRING, "-parent", NULL, NULL,			    \
        "0", offsetof(Tk_PathItem, parentObj), -1,		    \
	0, 0, PATH_CORE_OPTION_PARENT}

#define PATH_OPTION_SPEC_CORE(typeName)				    \
    {TK_OPTION_STRING_TABLE, "-state", NULL, NULL,		    \
        PATH_DEF_STATE, -1, offsetof(Tk_PathItem, state),	    \
        0, (ClientData) stateStrings, 0},			    \
    {TK_OPTION_STRING, "-style", (char *) NULL, (char *) NULL,	    \
	"", offsetof(typeName, styleObj), -1,			    \
	TK_OPTION_NULL_OK, 0, PATH_CORE_OPTION_STYLENAME},	    \
    {TK_OPTION_CUSTOM, "-tags", NULL, NULL,			    \
	NULL, -1, offsetof(Tk_PathItem, pathTagsPtr),		    \
	TK_OPTION_NULL_OK, (ClientData) &tagsCO, PATH_CORE_OPTION_TAGS}


#ifdef __cplusplus
}
#endif

#endif      /* INCLUDED_TKCANVPATHUTIL_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
