/*
 * tkIntPath.h --
 *
 *	Header file for the internals of the tkpath package.
 *
 * Copyright (c) 2005-2008  Mats Bengtsson
 *
 */

#ifndef INCLUDED_TKINTPATH_H
#define INCLUDED_TKINTPATH_H

#include "tkPath.h"
#include "tkCanvArrow.h"

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * From tclInt.h version 1.118.2.11
 * Ensure WORDS_BIGENDIAN is defined correcly:
 * Needs to happen here in addition to configure to work with
 * fat compiles on Darwin (i.e. ppc and i386 at the same time).
 */

#ifndef WORDS_BIGENDIAN
#	ifdef HAVE_SYS_TYPES_H
#		include <sys/types.h>
#	endif
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#   ifdef BYTE_ORDER
#		ifdef BIG_ENDIAN
#			if BYTE_ORDER == BIG_ENDIAN
#				define WORDS_BIGENDIAN
#			endif
#		endif
#		ifdef LITTLE_ENDIAN
#			if BYTE_ORDER == LITTLE_ENDIAN
#				undef WORDS_BIGENDIAN
#			endif
#		endif
#	endif
#endif


#ifndef MIN
#	define MIN(a, b) 	(((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#	define MAX(a, b) 	(((a) > (b)) ? (a) : (b))
#endif
#ifndef ABS
#	define ABS(a)    	(((a) >= 0)  ? (a) : -1*(a))
#endif
#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif
#define DEGREES_TO_RADIANS (M_PI/180.0)
#define RADIANS_TO_DEGREES (180.0/M_PI)


/*
 * This can be useful to estimate the segmentation detail necessary.
 * A conservative measure.
 */
#define TMATRIX_ABS_MAX(mPtr)		MAX(fabs(mPtr->a), MAX(fabs(mPtr->b), MAX(fabs(mPtr->c), fabs(mPtr->d))))

/*
 * This can be used for simplifying Area and Point functions.
 */
#define TMATRIX_IS_RECTILINEAR(mPtr)   	(fabs(mPtr->b) == 0.0) && (fabs(mPtr->c) == 0.0)

#define TMATRIX_DETERMINANT(mPtr)	(mPtr->a * mPtr->d - mPtr->c * mPtr->d)

/*
 * Iff stroke width is an integer, widthCode=1,2, move coordinate
 * to pixel boundary if even stroke width, widthCode=2,
 * or to pixel center if odd stroke width, widthCode=1.
 */
#define PATH_DEPIXELIZE(widthCode,x)     (!(widthCode) ? (x) : ((int) (floor((x) + 0.001)) + (((widthCode) == 1) ? 0.5 : 0)));

#define GetColorFromPathColor(pcol) 		(((pcol != NULL) && (pcol->color != NULL)) ? pcol->color : NULL )
#define GetGradientMasterFromPathColor(pcol)	(((pcol != NULL) && (pcol->gradientInstPtr != NULL)) ? pcol->gradientInstPtr->masterPtr : NULL )
#define HaveAnyFillFromPathColor(pcol) 		(((pcol != NULL) && ((pcol->color != NULL) || (pcol->gradientInstPtr != NULL))) ? 1 : 0 )

/*
 * So far we use a fixed number of straight line segments when
 * doing various things, but it would be better to use the de Castlejau
 * algorithm to iterate these segments.
 */
#define kPathNumSegmentsCurveTo     	18
#define kPathNumSegmentsQuadBezier 	12
#define kPathNumSegmentsMax		18
#define kPathNumSegmentsEllipse         48

#define kPathUnitTMatrix  {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}

/*
 * Flag bits for gradient and style changes.
 */
enum {
    PATH_GRADIENT_FLAG_CONFIGURE	= (1L << 0),
    PATH_GRADIENT_FLAG_DELETE
};

enum {
    PATH_STYLE_FLAG_CONFIGURE		= (1L << 0),
    PATH_STYLE_FLAG_DELETE
};

MODULE_SCOPE int gAntiAlias;

enum {
    kPathTextAnchorStart		= 0L,
    kPathTextAnchorMiddle,
    kPathTextAnchorEnd,
    kPathTextAnchorN,
    kPathTextAnchorW,
    kPathTextAnchorS,
    kPathTextAnchorE,
    kPathTextAnchorNW,
    kPathTextAnchorNE,
    kPathTextAnchorSW,
    kPathTextAnchorSE,
    kPathTextAnchorC
};

enum {
    kPathImageAnchorN = 0,
    kPathImageAnchorW,
    kPathImageAnchorS,
    kPathImageAnchorE,
    kPathImageAnchorNW,
    kPathImageAnchorNE,
    kPathImageAnchorSW,
    kPathImageAnchorSE,
    kPathImageAnchorC
};

enum {
    kPathImageInterpolationNone = 0,
    kPathImageInterpolationFast,
    kPathImageInterpolationBest
};
/* These MUST be kept in sync with methodST and unitsST! */
enum {
    kPathGradientMethodPad		= 0L,
    kPathGradientMethodRepeat,
    kPathGradientMethodReflect
};
enum {
    kPathGradientUnitsBoundingBox =	0L,
    kPathGradientUnitsUserSpace
};

enum {
    kPathArcOK,
    kPathArcLine,
    kPathArcSkip
};

typedef struct PathBox {
    double x;
    double y;
    double width;
    double height;
} PathBox;

typedef struct CentralArcPars {
    double cx;
    double cy;
    double rx;
    double ry;
    double theta1;
    double dtheta;
    double phi;
} CentralArcPars;

typedef struct LookupTable {
    int from;
    int to;
} LookupTable;

/*
 * Points in an arrowHead:
 */
#define PTS_IN_ARROW 6
#define DRAWABLE_PTS_IN_ARROW 5
#define ORIG_PT_IN_ARROW 2
#define LINE_PT_IN_ARROW 5

/*
 * Records used for parsing path to a linked list of primitive
 * drawing instructions.
 *
 * PathAtom: vaguely modelled after Tk_PathItem. Each atom has a PathAtom record
 * in its first position, padded with type specific data.
 */

typedef struct MoveToAtom {
    PathAtom pathAtom;		/* Generic stuff that's the same for all
                                 * types.  MUST BE FIRST IN STRUCTURE. */
    double x;
    double y;
} MoveToAtom;

typedef struct LineToAtom {
    PathAtom pathAtom;
    double x;
    double y;
} LineToAtom;

typedef struct ArcAtom {
    PathAtom pathAtom;
    double radX;
    double radY;
    double angle;		/* In degrees! */
    char largeArcFlag;
    char sweepFlag;
    double x;
    double y;
} ArcAtom;

typedef struct QuadBezierAtom {
    PathAtom pathAtom;
    double ctrlX;
    double ctrlY;
    double anchorX;
    double anchorY;
} QuadBezierAtom;

typedef struct CurveToAtom {
    PathAtom pathAtom;
    double ctrlX1;
    double ctrlY1;
    double ctrlX2;
    double ctrlY2;
    double anchorX;
    double anchorY;
} CurveToAtom;

typedef struct CloseAtom {
    PathAtom pathAtom;
    double x;
    double y;
} CloseAtom;

typedef struct EllipseAtom {
    PathAtom pathAtom;
    double cx;
    double cy;
    double rx;
    double ry;
} EllipseAtom;

typedef struct RectAtom {
    PathAtom pathAtom;
    double x;
    double y;
    double width;
    double height;
} RectAtom;

/*
 * Flags for 'TkPathStyleMergeStyles'.
 */

enum {
    kPathMergeStyleNotFill = 		0L,
    kPathMergeStyleNotStroke
};

/*
 * The actual path drawing commands which are all platform specific.
 */

MODULE_SCOPE TkPathContext TkPathInit(Tk_Window tkwin, Drawable d);
MODULE_SCOPE TkPathContext TkPathInitSurface(Display *display,
			int width, int height);
MODULE_SCOPE void   TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *stylePtr);
MODULE_SCOPE void   TkPathEndPath(TkPathContext ctx);
MODULE_SCOPE void   TkPathMoveTo(TkPathContext ctx, double x, double y);
MODULE_SCOPE void   TkPathLineTo(TkPathContext ctx, double x, double y);
MODULE_SCOPE void   TkPathArcTo(TkPathContext ctx, double rx, double ry,
			double angle, char largeArcFlag, char sweepFlag,
			double x, double y);
MODULE_SCOPE void   TkPathQuadBezier(TkPathContext ctx,
			double ctrlX, double ctrlY, double x, double y);
MODULE_SCOPE void   TkPathCurveTo(TkPathContext ctx,
			double ctrlX1, double ctrlY1,
			double ctrlX2, double ctrlY2, double x, double y);
MODULE_SCOPE void   TkPathArcToUsingBezier(TkPathContext ctx,
			double rx, double ry,
			double phiDegrees, char largeArcFlag, char sweepFlag,
			double x2, double y2);
MODULE_SCOPE void   TkPathRect(TkPathContext ctx, double x, double y,
			double width, double height);
MODULE_SCOPE void   TkPathOval(TkPathContext ctx, double cx, double cy,
			double rx, double ry);
MODULE_SCOPE void   TkPathClosePath(TkPathContext ctx);
MODULE_SCOPE void   TkPathImage(TkPathContext ctx, Tk_Image image,
			Tk_PhotoHandle photo,
			double x, double y, double width, double height,
			double fillOpacity,
			XColor *tintColor, double tintAmount,
			int interpolation, PathRect *srcRegion);
MODULE_SCOPE int    TkPathTextConfig(Tcl_Interp *interp,
			Tk_PathTextStyle *textStylePtr, char *utf8,
			void **customPtr);
MODULE_SCOPE void   TkPathTextDraw(TkPathContext ctx, Tk_PathStyle *style,
			Tk_PathTextStyle *textStylePtr, double x, double y,
			int fillOverStroke, char *utf8, void *custom);
MODULE_SCOPE void   TkPathTextFree(Tk_PathTextStyle *textStylePtr,
			 void *custom);
MODULE_SCOPE PathRect TkPathTextMeasureBbox(Display *display,
			Tk_PathTextStyle *textStylePtr, char *utf8,
			void *custom);
MODULE_SCOPE void   TkPathSurfaceErase(TkPathContext ctx, double x, double y,
			double width, double height);
MODULE_SCOPE void   TkPathSurfaceToPhoto(Tcl_Interp *interp,
			TkPathContext ctx, Tk_PhotoHandle photo);

/*
 * General path drawing using linked list of path atoms.
 */

MODULE_SCOPE void   TkPathDrawPath(Tk_Window tkwin, Drawable drawable,
			PathAtom *atomPtr, Tk_PathStyle *stylePtr,
			TMatrix *mPtr, PathRect *bboxPtr);
MODULE_SCOPE void   TkPathPaintPath(TkPathContext context, PathAtom *atomPtr,
			Tk_PathStyle *stylePtr, PathRect *bboxPtr);
MODULE_SCOPE PathRect TkPathGetTotalBbox(PathAtom *atomPtr,
			Tk_PathStyle *stylePtr);
MODULE_SCOPE void   TkPathMakePrectAtoms(double *pointsPtr,
			double rx, double ry, PathAtom **atomPtrPtr);
MODULE_SCOPE TkPathColor *TkPathNewPathColor(Tcl_Interp *interp,
			Tk_Window tkwin, Tcl_Obj *nameObj);
MODULE_SCOPE void   TkPathFreePathColor(TkPathColor *colorPtr);
MODULE_SCOPE TkPathColor *TkPathGetPathColor(Tcl_Interp *interp,
			Tk_Window tkwin, Tcl_Obj *nameObj,
			Tcl_HashTable *tablePtr,
			TkPathGradientChangedProc *changeProc,
			ClientData clientData);

/*
 * From the generic tk code normally in tkIntDecls.h
 */

MODULE_SCOPE void   TkPathIncludePoint(Tk_PathItem *itemPtr, double *pointPtr);
MODULE_SCOPE void   TkPathBezierScreenPoints(Tk_PathCanvas canvas,
			double control[], int numSteps, XPoint *xPointPtr);
MODULE_SCOPE void   TkPathBezierPoints(double control[], int numSteps,
			double *coordPtr);
MODULE_SCOPE int    TkPathMakeBezierCurve(Tk_PathCanvas canvas,
			double *pointPtr, int numPoints, int numSteps,
			XPoint xPoints[], double dblPoints[]);
MODULE_SCOPE int    TkPathMakeRawCurve(Tk_PathCanvas canvas, double *pointPtr,
			int numPoints, int numSteps, XPoint xPoints[],
			double dblPoints[]);
#ifndef TKP_NO_POSTSCRIPT
MODULE_SCOPE void   TkPathMakeBezierPostscript(Tcl_Interp *interp,
			Tk_PathCanvas canvas,
			double *pointPtr, int numPoints);
MODULE_SCOPE void   TkPathMakeRawCurvePostscript(Tcl_Interp *interp,
			Tk_PathCanvas canvas,
			double *pointPtr, int numPoints);
#endif
MODULE_SCOPE void   TkPathFillPolygon(Tk_PathCanvas canvas, double *coordPtr,
			int numPoints, Display *display,
			Drawable drawable, GC gc, GC outlineGC);

/*
 * Various stuff.
 */

MODULE_SCOPE int    TableLookup(LookupTable *map, int n, int from);
MODULE_SCOPE void   PathParseDashToArray(Tk_Dash *dash, double width, int *len,
			float **arrayPtrPtr);
MODULE_SCOPE void   PathApplyTMatrix(TMatrix *m, double *x, double *y);
MODULE_SCOPE void   PathApplyTMatrixToPoint(TMatrix *m, double in[2],
			double out[2]);
MODULE_SCOPE void   PathInverseTMatrix(TMatrix *m, TMatrix *mi);
MODULE_SCOPE void   MMulTMatrix(TMatrix *m1, TMatrix *m2);
MODULE_SCOPE void   PathCopyBitsARGB(unsigned char *from, unsigned char *to,
			int width, int height, int bytesPerRow);
MODULE_SCOPE void   PathCopyBitsBGRA(unsigned char *from, unsigned char *to,
			int width, int height, int bytesPerRow);
MODULE_SCOPE void   PathCopyBitsPremultipliedAlphaRGBA(unsigned char *from,
			unsigned char *to,
			int width, int height, int bytesPerRow);
MODULE_SCOPE void   PathCopyBitsPremultipliedAlphaARGB(unsigned char *from,
			unsigned char *to,
			int width, int height, int bytesPerRow);
MODULE_SCOPE void   PathCopyBitsPremultipliedAlphaBGRA(unsigned char *from,
			unsigned char *to,
			int width, int height, int bytesPerRow);
MODULE_SCOPE int    ObjectIsEmpty(Tcl_Obj *objPtr);
MODULE_SCOPE int    PathGetTMatrix(Tcl_Interp* interp, const char *list,
			TMatrix *matrixPtr);
MODULE_SCOPE int    PathGetTclObjFromTMatrix(Tcl_Interp* interp,
			TMatrix *matrixPtr, Tcl_Obj **listObjPtrPtr);
MODULE_SCOPE int    EndpointToCentralArcParameters(
			/* Endpoints. */
			double x1, double y1, double x2, double y2,
			/* Radius. */
			double rx, double ry,
			double phi, char largeArcFlag, char sweepFlag,
			/* Output. */
			double *cxPtr, double *cyPtr,
			double *rxPtr, double *ryPtr,
			double *theta1Ptr, double *dthetaPtr);
MODULE_SCOPE int    TkPathGenericCmdDispatcher(
			Tcl_Interp* interp, Tk_Window tkwin,
			int objc, Tcl_Obj* const objv[],
			char *baseName, int *baseNameUIDPtr,
			Tcl_HashTable *hashTablePtr,
			Tk_OptionTable optionTable,
			char *(*createAndConfigProc)(Tcl_Interp *interp,
			    char *name, int objc, Tcl_Obj *const objv[]),
			void (*configNotifyProc)(char *recordPtr, int mask,
			    int objc, Tcl_Obj *const objv[]),
			void (*freeProc)(Tcl_Interp *interp, char *recordPtr));
MODULE_SCOPE void   PathStyleInit(Tcl_Interp* interp);
MODULE_SCOPE void   PathGradientInit(Tcl_Interp* interp);
MODULE_SCOPE void   TkPathStyleMergeStyles(Tk_PathStyle *srcStyle,
			Tk_PathStyle *dstStyle,
			long flags);
MODULE_SCOPE int    TkPathStyleMergeStyleStatic(Tcl_Interp* interp,
			Tcl_Obj *styleObj,
			Tk_PathStyle *dstStyle, long flags);
MODULE_SCOPE void   PathGradientPaint(TkPathContext ctx, PathRect *bbox,
			TkPathGradientMaster *gradientStylePtr,
			int fillRule, double fillOpacity);
MODULE_SCOPE Tk_PathSmoothMethod	tkPathBezierSmoothMethod;
MODULE_SCOPE int    Tk_PathCanvasObjCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

/*
 * Gradient support functions.
 */

MODULE_SCOPE int    PathGradientCget(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE int    PathGradientConfigure(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE int    PathGradientCreate(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *hashTablePtr, char *tokenName);
MODULE_SCOPE int    PathGradientDelete(Tcl_Interp *interp, Tcl_Obj *obj,
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE int    PathGradientInUse(Tcl_Interp *interp, Tcl_Obj *obj,
			Tcl_HashTable *tablePtr);
MODULE_SCOPE void   PathGradientNames(Tcl_Interp *interp,
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE int    PathGradientType(Tcl_Interp *interp, Tcl_Obj *obj,
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE void   PathGradientMasterFree(TkPathGradientMaster *gradientPtr);

/*
 * Style support functions.
 */

MODULE_SCOPE int    PathStyleCget(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *hashTablePtr);
MODULE_SCOPE int    PathStyleConfigure(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *hashTablePtr,
			Tcl_HashTable *gradTablePtr);
MODULE_SCOPE int    PathStyleCreate(Tcl_Interp *interp, Tk_Window tkwin,
			int objc, Tcl_Obj * const objv[],
			Tcl_HashTable *styleTablePtr,
			Tcl_HashTable *gradTablePtr, char *tokenName);
MODULE_SCOPE int    PathStyleDelete(Tcl_Interp *interp, Tcl_Obj *obj,
			Tcl_HashTable *hashTablePtr, Tk_Window tkwin);
MODULE_SCOPE int    PathStyleInUse(Tcl_Interp *interp, Tcl_Obj *obj,
			Tcl_HashTable *tablePtr);
MODULE_SCOPE void   PathStyleNames(Tcl_Interp *interp,
			Tcl_HashTable *hashTablePtr);

/*
 * As TK_OPTION_PIXELS but double internal value instead of int.
 */

MODULE_SCOPE int	Tk_PathPixelOptionSetProc(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    Tcl_Size internalOffset, char *oldInternalPtr,
			    int flags);
MODULE_SCOPE Tcl_Obj *	Tk_PathPixelOptionGetProc(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    Tcl_Size internalOffset);
MODULE_SCOPE void	Tk_PathPixelOptionRestoreProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr,
			    char *oldInternalPtr);
MODULE_SCOPE int	Tk_DashOptionSetProc(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    Tcl_Size internalOffset, char *oldInternalPtr,
			    int flags);
MODULE_SCOPE Tcl_Obj *	Tk_DashOptionGetProc(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    Tcl_Size internalOffset);
MODULE_SCOPE void	Tk_DashOptionRestoreProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr,
			    char *oldInternalPtr);
MODULE_SCOPE void	Tk_DashOptionFreeProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr);
MODULE_SCOPE int	TkPathOffsetOptionSetProc(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    Tcl_Size internalOffset, char *oldInternalPtr,
			    int flags);
MODULE_SCOPE Tcl_Obj *	TkPathOffsetOptionGetProc(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    Tcl_Size internalOffset);
MODULE_SCOPE void	TkPathOffsetOptionRestoreProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr,
			    char *oldInternalPtr);
MODULE_SCOPE void	TkPathOffsetOptionFreeProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr);
MODULE_SCOPE int	TkPathSmoothOptionSetProc(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    Tcl_Size internalOffset, char *oldInternalPtr,
			    int flags);
MODULE_SCOPE Tcl_Obj *	TkPathSmoothOptionGetProc(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    Tcl_Size internalOffset);
MODULE_SCOPE void	TkPathSmoothOptionRestoreProc(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr,
			    char *oldInternalPtr);
#ifndef TKP_NO_POSTSCRIPT
MODULE_SCOPE int	TkPathPostscriptImage(Tcl_Interp *interp,
			    Tk_Window tkwin,
			    Tk_PostscriptInfo psInfo, XImage *ximage,
			    int x, int y, int width, int height);
#endif
MODULE_SCOPE void	PathStylesFree(Tk_Window tkwin,
			    Tcl_HashTable *hashTablePtr);
MODULE_SCOPE TkPathColor * TkPathGetPathColorStatic(Tcl_Interp *interp,
			    Tk_Window tkwin, Tcl_Obj *nameObj);

/*
 * Support functions for gradient instances.
 */

MODULE_SCOPE TkPathGradientInst *TkPathGetGradient(Tcl_Interp *interp,
			    const char *name,
			    Tcl_HashTable *tablePtr,
			    TkPathGradientChangedProc *changeProc,
			    ClientData clientData);
MODULE_SCOPE void	TkPathFreeGradient(TkPathGradientInst *gradientPtr);
MODULE_SCOPE void	TkPathGradientChanged(TkPathGradientMaster *masterPtr,
			    int flags);

MODULE_SCOPE void	TkPathStyleChanged(Tk_PathStyle *masterPtr, int flags);

/*
 * Support functions to generate pdf output.
 */

MODULE_SCOPE int	TkPathPdf(Tcl_Interp *interp, PathAtom *atomPtr,
			    Tk_PathStyle *stylePtr, PathRect *bboxPtr,
			    int objc, Tcl_Obj *const objv[]);
MODULE_SCOPE int	TkPathPdfArrow(Tcl_Interp *interp, ArrowDescr *arrow,
			    Tk_PathStyle *const style);
MODULE_SCOPE int	TkPathPdfColor(Tcl_Obj *ret, XColor *colorPtr,
			    const char *command);
MODULE_SCOPE Tcl_Obj *	TkPathExtGS(Tk_PathStyle *stylePtr, long *smaskRef);
MODULE_SCOPE int	TkPathPdfNumber(Tcl_Obj *ret, int fracDigis,
			    double number, const char *append);

/*
 * end block for C++
 */

#ifdef __cplusplus
}
#endif

#endif      /* INCLUDED_TKINTPATH_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
