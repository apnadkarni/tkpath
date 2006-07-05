/*
 * tkMacOSXPath.c --
 *
 *	This file implements path drawing API's using CoreGraphics on Mac OS X.
 *
 * Copyright (c) 2005-2006  Mats Bengtsson
 *
 * $Id$
 *
 */

#include "tkMacOSXInt.h"
#include "tkPath.h"
#include "tkIntPath.h"

#ifdef WORDS_BIGENDIAN
#	define BlueFloatFromXColorPtr(xc)   (float) ((((xc)->pixel >> 0)  & 0xFF)) / 255.0
#	define GreenFloatFromXColorPtr(xc)  (float) ((((xc)->pixel >> 8)  & 0xFF)) / 255.0
#	define RedFloatFromXColorPtr(xc)    (float) ((((xc)->pixel >> 16) & 0xFF)) / 255.0
#else
#	define BlueFloatFromXColorPtr(xc)   (float) ((((xc)->pixel >> 16) & 0xFF)) / 255.0
#	define GreenFloatFromXColorPtr(xc)  (float) ((((xc)->pixel >> 8)  & 0xFF)) / 255.0
#	define RedFloatFromXColorPtr(xc)    (float) ((((xc)->pixel >> 0)  & 0xFF)) / 255.0
#endif

extern int gUseAntiAlias;

/* For debugging. */
extern Tcl_Interp *gInterp;

/*
 * This is used as a place holder for platform dependent stuff between each call.
 */
typedef struct TkPathContext_ {
    Drawable 		d;
    CGContextRef 	c;
} TkPathContext_;

void
PathSetUpCGContext(    
        MacDrawable *macWin,
        CGrafPtr destPort,
        CGContextRef *contextPtr)
{
    CGContextRef outContext;
    OSStatus err;
    Rect boundsRect;
    CGAffineTransform transform;

    err = QDBeginCGContext(destPort, contextPtr);
    outContext = *contextPtr;
    
    CGContextSaveGState(outContext);
    
    GetPortBounds(destPort, &boundsRect);
    
    CGContextResetCTM(outContext);
    transform = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0, 
            (float)(boundsRect.bottom - boundsRect.top));
    CGContextConcatCTM(outContext, transform);
    
    CGContextSetShouldAntialias(outContext, gUseAntiAlias);
    CGContextSetInterpolationQuality(outContext, kCGInterpolationHigh);
    
    /* Since we are using Pixmaps only we need no clipping or shifting. */
}

void
PathReleaseCGContext(
        MacDrawable *macWin,
        CGrafPtr destPort, 
        CGContextRef *outContext)
{
    CGContextResetCTM(*outContext);
    CGContextRestoreGState(*outContext);
    QDEndCGContext(destPort, outContext);
}

CGColorSpaceRef GetTheColorSpaceRef(void)
{
    static CGColorSpaceRef deviceRGB = NULL;
    if (deviceRGB == NULL) {
        deviceRGB = CGColorSpaceCreateDeviceRGB();
    }
    return deviceRGB;
}

#if 0	// 10.3
/* Cache some common colors to speed things up. */
typedef struct LookupColor {
    int from;
    CGColorRef colorRef;
} LookupTable;
static LookupColor ColorTable[] = {

};
void
PreallocateColorRefs(void)
{

}
#endif

static LookupTable LineCapStyleLookupTable[] = {
    {CapNotLast, 		kCGLineCapButt},
    {CapButt, 	 		kCGLineCapButt},
    {CapRound, 	 		kCGLineCapRound},
    {CapProjecting, 	kCGLineCapSquare}
};

static LookupTable LineJoinStyleLookupTable[] = {
    {JoinMiter, 	kCGLineJoinMiter},
    {JoinRound,		kCGLineJoinRound},
    {JoinBevel, 	kCGLineJoinBevel}
};

void
PathSetCGContextStyle(CGContextRef c, Tk_PathStyle *style)
{
    Tk_Dash *dash;
    
    /** Drawing attribute functions. **/
    
    /* Set the line width in the current graphics state to `width'. */    
    CGContextSetLineWidth(c, style->strokeWidth);
    
    /* Set the line cap in the current graphics state to `cap'. */
    CGContextSetLineCap(c, 
            TableLookup(LineCapStyleLookupTable, 4, style->capStyle));

    /* Set the line join in the current graphics state to `join'. */
    CGContextSetLineJoin(c,
            TableLookup(LineJoinStyleLookupTable, 3, style->joinStyle));
    
    /* Set the miter limit in the current graphics state to `limit'. */
    CGContextSetMiterLimit(c, style->miterLimit);

    /* Set the line dash patttern in the current graphics state. */
    dash = &(style->dash);
    if ((dash != NULL) && (dash->number != 0)) {
        int	len;
        float 	phase;
        float 	*array;
    
        PathParseDashToArray(dash, style->strokeWidth, &len, &array);
        if (len > 0) {
            phase = 0.0;
            CGContextSetLineDash(c, phase, array, len);
            ckfree((char *) array);
        }
    }
    
    /* Set the current fill colorspace in the context `c' to `DeviceRGB' and
    * set the components of the current fill color to `(red, green, blue,
    * alpha)'. */
    if (style->fillColor != NULL) {
        CGContextSetRGBFillColor(c, 
                RedFloatFromXColorPtr(style->fillColor), 
                GreenFloatFromXColorPtr(style->fillColor),
                BlueFloatFromXColorPtr(style->fillColor),
                style->fillOpacity);
    }
    
    /* Set the current stroke colorspace in the context `c' to `DeviceRGB' and
    * set the components of the current stroke color to `(red, green, blue,
    * alpha)'. */
    if (style->strokeColor != NULL) {
        CGContextSetRGBStrokeColor(c, 
                RedFloatFromXColorPtr(style->strokeColor), 
                GreenFloatFromXColorPtr(style->strokeColor),
                BlueFloatFromXColorPtr(style->strokeColor),
                style->strokeOpacity);
    }
    if (style->fillStipple != None) {
        /* @@@ TODO */
        //CGContextSetFillPattern(c, CGPatternRef pattern, const float color[]);
    }
    if (style->strokeStipple != None) {
        /* @@@ TODO */
        //CGContextSetStrokePattern(c, CGPatternRef pattern, const float color[]);
    }
}

TkPathContext	
TkPathInit(Tk_Window tkwin, Drawable d)
{
    CGContextRef cgContext;
    TkPathContext_ *context = (TkPathContext_ *) ckalloc((unsigned) (sizeof(TkPathContext_)));
    
    PathSetUpCGContext((MacDrawable *) d, TkMacOSXGetDrawablePort(d), &cgContext);
    context->d = d;
    context->c = cgContext;
    return (TkPathContext) context;
}

void
TkPathPushTMatrix(TkPathContext ctx, TMatrix *mPtr)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGAffineTransform transform;

    /* Return the transform [ a b c d tx ty ]. */
    transform = CGAffineTransformMake(
            (float) mPtr->a, (float) mPtr->b,
            (float) mPtr->c, (float) mPtr->d,
            (float) mPtr->tx, (float) mPtr->ty);
    CGContextConcatCTM(context->c, transform);    
}

void
TkPathBeginPath(TkPathContext ctx, Tk_PathStyle *stylePtr)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextBeginPath(context->c);
    PathSetCGContextStyle(context->c, stylePtr);
}

void
TkPathMoveTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextMoveToPoint(context->c, x, y);
}

void
TkPathLineTo(TkPathContext ctx, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextAddLineToPoint(context->c, x, y);
}

void
TkPathLinesTo(TkPathContext ctx, double *pts, int n)
{
    //TkPathContext_ *context = (TkPathContext_ *) ctx;
    /* Add a set of lines to the context's path. */
    //CGContextAddLines(context->c, const CGPoint points[], size_t count);
}

void
TkPathQuadBezier(TkPathContext ctx, double ctrlX, double ctrlY, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextAddQuadCurveToPoint(context->c, ctrlX, ctrlY, x, y);
}

void
TkPathCurveTo(TkPathContext ctx, double ctrlX1, double ctrlY1, 
        double ctrlX2, double ctrlY2, double x, double y)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextAddCurveToPoint(context->c, ctrlX1, ctrlY1, ctrlX2, ctrlY2, x, y);
}

void
TkPathArcTo(TkPathContext ctx,
        double rx, double ry, 
        double phiDegrees, 	/* The rotation angle in degrees! */
        char largeArcFlag, char sweepFlag, double x, double y)
{
    //TkPathContext_ *context = (TkPathContext_ *) ctx;
    // @@@ Should we try to use the native arc functions here?
    TkPathArcToUsingBezier(ctx, rx, ry, phiDegrees, largeArcFlag, sweepFlag, x, y);
}

void
TkPathRect(TkPathContext ctx, double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGRect r;
    r = CGRectMake(x, y, width, height);
    CGContextAddRect(context->c, r);
}

void
TkPathOval(TkPathContext ctx, double cx, double cy, double rx, double ry)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;

#if 0	// 10.4
    if (&CGContextAddEllipseInRect != NULL) {
        CGRect r;
        r = CGRectMake(cx-rx, cy-ry, 2*rx, 2*ry);
        CGContextAddEllipseInRect(context->c, r);
    } else {
#endif
    if (rx == ry) {
        CGContextMoveToPoint(context->c, cx+rx, cy);
        CGContextAddArc(context->c, cx, cy, rx, 0.0, 2*M_PI, 1);
        CGContextClosePath(context->c);
    } else {
        CGContextSaveGState(context->c);
        CGContextTranslateCTM(context->c, cx, cy);
        CGContextScaleCTM(context->c, rx, ry);
        CGContextMoveToPoint(context->c, 1, 0);
        CGContextAddArc(context->c, 0.0, 0.0, 1.0, 0.0, 2*M_PI, 1);
        CGContextRestoreGState(context->c);
        CGContextClosePath(context->c);
    }
}

void
TkPathImage(TkPathContext ctx, Tk_Image image, Tk_PhotoHandle photo, 
        double x, double y, double width, double height)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGRect rectangle;
    CGImageRef cgImage;
    CGDataProviderRef provider;
    CGColorSpaceRef colorspace;
    CGImageAlphaInfo alphaInfo;
    size_t size;
    Tk_PhotoImageBlock block;
    
    /* Return value? */
    Tk_PhotoGetImage(photo, &block);
    size = block.pitch * block.height;
    
    /*
     * The offset array contains the offsets from the address of a pixel to 
     * the addresses of the bytes containing the red, green, blue and alpha 
     * (transparency) components.  These are normally 0, 1, 2 and 3. 
     * @@@ There are more cases to consider than these!
     */
    if (block.offset[3] == 3) {
        alphaInfo = kCGImageAlphaLast;
    } else if (block.offset[3] == 0) {
        alphaInfo = kCGImageAlphaFirst;
    } else {
        /* @@@ What to do here? */
        return;
    }
    provider = CGDataProviderCreateWithData(NULL, block.pixelPtr, size, NULL);
    colorspace = CGColorSpaceCreateDeviceRGB();
    cgImage = CGImageCreate(block.width, block.height, 
            8, 						/* bitsPerComponent */
            block.pixelSize*8,	 	/* bitsPerPixel */
            block.pitch, 			/* bytesPerRow */
            colorspace,				/* colorspace */
            alphaInfo,				/* alphaInfo */
            provider, NULL, 
            1, 						/* shouldInterpolate */
            kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorspace);
    if (width == 0.0) {
        width = (double) block.width;
    }
    if (height == 0.0) {
        height = (double) block.height;
    }
    
    /* Flip back to an upright coordinate system since CGContextDrawImage expect this. */
    CGContextSaveGState(context->c);
    CGContextTranslateCTM(context->c, x, y+height);
    CGContextScaleCTM(context->c, 1, -1);
    rectangle = CGRectMake(0.0, 0.0, width, height);
    CGContextDrawImage(context->c, rectangle, cgImage);
    CGImageRelease(cgImage);
    CGContextRestoreGState(context->c);
}

void
TkPathClosePath(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextClosePath(context->c);
}

void		
TkPathClipToPath(TkPathContext ctx, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;

    /* If you need to grow the clipping path after it’s shrunk, you must save the
     * graphics state before you clip, then restore the graphics state to restore the current
     * clipping path. */
    CGContextSaveGState(context->c);
    if (fillRule == WindingRule) {
        CGContextClip(context->c);
    } else if (fillRule == EvenOddRule) {
        CGContextEOClip(context->c);
    }
}

void
TkPathReleaseClipToPath(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextRestoreGState(context->c);
}

void
TkPathStroke(TkPathContext ctx, Tk_PathStyle *style)
{       
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGContextStrokePath(context->c);
}

void
TkPathFill(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    if (style->fillRule == WindingRule) {
        CGContextFillPath(context->c);
    } else if (style->fillRule == EvenOddRule) {
        CGContextEOFillPath(context->c);
    }
}

void        
TkPathFillAndStroke(TkPathContext ctx, Tk_PathStyle *style)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    if (style->fillRule == WindingRule) {
        CGContextDrawPath(context->c, kCGPathFillStroke);
    } else if (style->fillRule == EvenOddRule) {
        CGContextDrawPath(context->c, kCGPathEOFillStroke);
    }
}

void
TkPathEndPath(TkPathContext ctx)
{
    //TkPathContext_ *context = (TkPathContext_ *) ctx;
    /* Empty ??? */
}

void
TkPathFree(TkPathContext ctx)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    PathReleaseCGContext((MacDrawable *) context->d, TkMacOSXGetDrawablePort(context->d), &(context->c));
    ckfree((char *) ctx);
}

int		
TkPathDrawingDestroysPath(void)
{
    return 1;
}

/* TkPathGetCurrentPosition --
 *
 * 		Returns the current pen position in untransformed coordinates!
 */
 
int		
TkPathGetCurrentPosition(TkPathContext ctx, PathPoint *ptPtr)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGPoint cgpt;
    
    cgpt = CGContextGetPathCurrentPoint(context->c);
    ptPtr->x = cgpt.x;
    ptPtr->y = cgpt.y;
    return TCL_OK;
}

int 
TkPathBoundingBox(TkPathContext ctx, PathRect *rPtr)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    CGRect cgRect;
    
    /* This one is not very useful since it includes the control points. */
    cgRect = CGContextGetPathBoundingBox(context->c);
    rPtr->x1 = cgRect.origin.x;
    rPtr->y1 = cgRect.origin.y;
    rPtr->x2 = cgRect.origin.x + cgRect.size.width;
    rPtr->y2 = cgRect.origin.y + cgRect.size.height;
    return TCL_OK;
}

/*
 * Using CGShading for fill gradients.
 */
 
/* @@@ Perhaps it would be better to make the complete shading in a single call
       instead of dividing it up into two stop calls?
*/
 
typedef struct TwoStopRecord {
    GradientStop *stop1;
    GradientStop *stop2;
} TwoStopRecord;

static void
ShadeEvaluate(void *info, const float *in, float *out)
{
    TwoStopRecord 	*twoStopPtr = (TwoStopRecord *) info;
    float 			par = *in;
    float			par2;
    GradientStop 	*stop1, *stop2;
    
    stop1 = twoStopPtr->stop1;
    stop2 = twoStopPtr->stop2;
        
    /* Interpolate between the two stops. */
    par2 = 1.0 - par;
    *out++ = par2 * RedFloatFromXColorPtr(stop1->color) + 
            par * RedFloatFromXColorPtr(stop2->color);
    *out++ = par2 * GreenFloatFromXColorPtr(stop1->color) + 
            par * GreenFloatFromXColorPtr(stop2->color);
    *out++ = par2 * BlueFloatFromXColorPtr(stop1->color) + 
            par * BlueFloatFromXColorPtr(stop2->color);
    *out++ = par2 * stop1->opacity + par * stop2->opacity;
}

static void
ShadeRelease(void *info)
{
    /* Not sure if anything to do here. */
}

void
TkPathPaintLinearGradient(TkPathContext ctx, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    int					i, nstops;
    int					fillMethod;
    bool 				extendStart, extendEnd;
    CGShadingRef 		shading;
    CGPoint 			start, end;
    CGColorSpaceRef 	colorSpaceRef;
    CGFunctionRef 		function;
    CGFunctionCallbacks callbacks;
    PathRect 			*trans;		/* The transition line. */
    GradientStop 		*stop1, *stop2;
    TwoStopRecord		twoStop;
    GradientStopArray 	*stopArrPtr;
    
    trans = fillPtr->transitionPtr;
    stopArrPtr = fillPtr->stopArrPtr;
    nstops = stopArrPtr->nstops;
    fillMethod = fillPtr->method;
    
    callbacks.version = 0;
    callbacks.evaluate = ShadeEvaluate;
    callbacks.releaseInfo = ShadeRelease;
    colorSpaceRef = CGColorSpaceCreateDeviceRGB();

    /*
     * We need to do like this since this is how SVG defines gradient drawing.
     */
    CGContextSaveGState(context->c);
    CGContextTranslateCTM(context->c, bbox->x1, bbox->y1);
    CGContextScaleCTM(context->c, bbox->x2 - bbox->x1, bbox->y2 - bbox->y1);
    
    /*
     * Paint all stops pairwise.
     */
    for (i = 0; i < nstops - 1; i++) {
        stop1 = stopArrPtr->stops[i];
        stop2 = stopArrPtr->stops[i+1];
        twoStop.stop1 = stop1;
        twoStop.stop2 = stop2;
        function = CGFunctionCreate((void *) &twoStop, 1, NULL, 4, NULL, &callbacks);
        
        /* If the two offsets identical then skip. */
        if (fabs(stop1->offset - stop2->offset) < 1e-6) {
            continue;
        }
        extendStart = 0;
        extendEnd = 0;
        if (i == 0) {
            extendStart = 1;
        }  
        if (i == nstops-2) {
            extendEnd = 1;
        }
        start.x = trans->x1 + stop1->offset * (trans->x2 - trans->x1);
        start.y = trans->y1 + stop1->offset * (trans->y2 - trans->y1);
        end.x   = trans->x1 + stop2->offset * (trans->x2 - trans->x1);
        end.y   = trans->y1 + stop2->offset * (trans->y2 - trans->y1);
    
        shading = CGShadingCreateAxial(colorSpaceRef, start, end, function, extendStart, extendEnd);
        CGContextDrawShading(context->c, shading);
        CGShadingRelease(shading);
        CGFunctionRelease(function);
    }
    CGColorSpaceRelease(colorSpaceRef);
    CGContextRestoreGState(context->c);
}

void
TkPathPaintRadialGradient(TkPathContext ctx, PathRect *bbox, RadialGradientFill *fillPtr, int fillRule)
{
    TkPathContext_ *context = (TkPathContext_ *) ctx;
    int					i, nstops;
    int					fillMethod;
    float 				startRadius, endRadius;
    bool 				extendStart, extendEnd;
    CGShadingRef 		shading;
    CGPoint 			start, end;
    CGColorSpaceRef 	colorSpaceRef;
    CGFunctionRef 		function;
    CGFunctionCallbacks callbacks;
    RadialTransition    *tPtr;
    GradientStop 		*stop1, *stop2;
    TwoStopRecord		twoStop;
    GradientStopArray 	*stopArrPtr;

    stopArrPtr = fillPtr->stopArrPtr;
    nstops = stopArrPtr->nstops;
    tPtr = fillPtr->radialPtr;
    
    callbacks.version = 0;
    callbacks.evaluate = ShadeEvaluate;
    callbacks.releaseInfo = ShadeRelease;
    colorSpaceRef = CGColorSpaceCreateDeviceRGB();

    /*
     * We need to do like this since this is how SVG defines gradient drawing.
     */
    CGContextSaveGState(context->c);
    CGContextTranslateCTM(context->c, bbox->x1, bbox->y1);
    CGContextScaleCTM(context->c, bbox->x2 - bbox->x1, bbox->y2 - bbox->y1);
    
    /*
     * Paint all stops pairwise.
     */
    for (i = 0; i < nstops - 1; i++) {
        stop1 = stopArrPtr->stops[i];
        stop2 = stopArrPtr->stops[i+1];
        twoStop.stop1 = stop1;
        twoStop.stop2 = stop2;
        function = CGFunctionCreate((void *) &twoStop, 1, NULL, 4, NULL, &callbacks);
        
        /* If the two offsets identical then skip. */
        if (fabs(stop1->offset - stop2->offset) < 1e-6) {
            continue;
        }
        extendStart = 0;
        extendEnd = 0;
        if (i == 0) {
            extendStart = 1;
        }  
        if (i == nstops-2) {
            extendEnd = 1;
        }
        start.x = tPtr->focalX + stop1->offset * (tPtr->centerX - tPtr->focalX);
        start.y = tPtr->focalY + stop1->offset * (tPtr->centerY - tPtr->focalY);
        end.x   = tPtr->focalX + stop2->offset * (tPtr->centerX - tPtr->focalX);
        end.y   = tPtr->focalY + stop2->offset * (tPtr->centerY - tPtr->focalY);
        startRadius = tPtr->radius * stop1->offset;
        endRadius = tPtr->radius * stop2->offset;

        shading = CGShadingCreateRadial(colorSpaceRef, start, startRadius, end, endRadius, function, extendStart, extendEnd);
        CGContextDrawShading(context->c, shading);
        CGShadingRelease(shading);
        CGFunctionRelease(function);
    }    
    CGColorSpaceRelease(colorSpaceRef);
    CGContextRestoreGState(context->c);
}

/*-------- This is replaced by Shading!!! -----------------*/

/* OUTDATED!!!!!!!!!! */

#if 0

static void
FillTwoStopLinearGradient(
        Drawable d, 
        PathRect *bbox, /* The items bounding box. */
        PathRect *line,	/* The relative line that defines the
                         * gradient transition line. 
                         * We paint perp to this line. */
        XColor *color1, XColor *color2, float opacity1, float opacity2)
{
    int i, nsteps;
    int lengthInt;
    float x, dx, width, relative;
    float rgba1[4], rgba2[4], deltaRGBA[4];
    float maxDeltaRGBA;
    double centerX, centerY;
    double lineX1, lineX2, lineY1, lineY2;
    double angle, length, diag;
    CGRect rect;
    
    /* Maximum paint length. */
    diag = hypot(bbox->x2 - bbox->x1, bbox->y2 - bbox->y1);
    if (diag < 0.1) {
        return;
    }
    diag += 2.0;
    
    /* Center of bounding box. */
    centerX = (bbox->x1 + bbox->x2)/2.0;
    centerY = (bbox->y1 + bbox->y2)/2.0;
    
    /* Scale up 'line' vector to bbox. */
    lineX1 = bbox->x1 + (bbox->x2 - bbox->x1)*line->x1;
    lineY1 = bbox->y1 + (bbox->y2 - bbox->y1)*line->y1;
    lineX2 = bbox->x1 + (bbox->x2 - bbox->x1)*line->x2;
    lineY2 = bbox->y1 + (bbox->y2 - bbox->y1)*line->y2;
    length = hypot(lineX2 - lineX1, lineY2 - lineY1);
    lengthInt = (int) (length + 0.5);
    if (lengthInt < 1) {
        return;
    }
    
    /* Angle of 'line' vector. */
    angle = atan2(lineY2 - lineY1, lineX2 - lineX1);
    
    /* Color differences. */
    rgba1[0] = RedFloatFromXColorPtr(color1);
    rgba1[1] = GreenFloatFromXColorPtr(color1);
    rgba1[2] = BlueFloatFromXColorPtr(color1);
    rgba1[3] = opacity1;
    rgba2[0] = RedFloatFromXColorPtr(color2);
    rgba2[1] = GreenFloatFromXColorPtr(color2);
    rgba2[2] = BlueFloatFromXColorPtr(color2);
    rgba2[3] = opacity2;
    maxDeltaRGBA = 0.0;
    for (i = 0; i < 4; i++) {
        deltaRGBA[i] = rgba2[i] - rgba1[i];
        if (fabs(deltaRGBA[i]) > maxDeltaRGBA) {
            maxDeltaRGBA = fabs(deltaRGBA[i]);
        }
    }
    
    /* Move coordinate system to first 'line' point and rotate it
     * in the direction of the 'line' vector. */
    CGContextSaveGState(gPathCGContext);
    CGContextSetShouldAntialias(gPathCGContext, 0);
    CGContextTranslateCTM(gPathCGContext, lineX1, lineY1);
    CGContextRotateCTM(gPathCGContext, angle);
    
    /* The number of rectangles to paint with and still
     * keep a smooth gradient. */
    nsteps = MAX(1, MIN(lengthInt, (int) (256.0*maxDeltaRGBA + 1.0)));
    dx = MAX(1, length/nsteps);
    width = ((float) lengthInt)/((float) nsteps) + 1.0;

    rect.origin.y = diag;
    rect.size.height = -2.0*diag;
    
    /* Note: CGContextFillRect() clears the current path! ??? */
    for (i = 0, x = 0.0; i < nsteps; i++, x += dx) {
        rect.origin.x = x;
        if (x + width > length) {
            rect.size.width = length - x;
        } else {
            rect.size.width = width;
        }
        relative = x/length;
        CGContextSetRGBFillColor(gPathCGContext,
                rgba1[0] + relative*deltaRGBA[0],
                rgba1[1] + relative*deltaRGBA[1],
                rgba1[2] + relative*deltaRGBA[2],
                rgba1[3] + relative*deltaRGBA[3]);
        CGContextFillRect(gPathCGContext, rect);
    }
    CGContextRestoreGState(gPathCGContext);
}

/*
 * This code is a bit too general to be placed in platform specific file,
 * but the problem is that setting the clip path resets the path
 * which therefore needs to be redefined. This is needed if we need to
 * stroke it after this. ??????????????
 */
 
#define FloatsEqual(a, b) ((fabs((a) - (b)) < 1e-6) ? 1 : 0)

enum {
    kPathGradientPadNone 	= 0L,
    kPathGradientPadFirst,
    kPathGradientPadSecond,
    kPathGradientPadBoth
};

/* Find out if a line defining a linear gradient requires padding at its ends. */

static int
NeedTransitionPadding(PathRect *line)
{
    int xAtBorders = 0, yAtBorders = 0;
    int padFirst = 0, padSecond = 0;
    double x1, y1, x2, y2;
    
    x1 = line->x1;
    y1 = line->y1;
    x2 = line->x2;
    y2 = line->y2;
    
    if ((FloatsEqual(x1, 0.0) && FloatsEqual(x2, 1.0)) ||
            (FloatsEqual(x1, 1.0) && FloatsEqual(x2, 0.0))) {
        xAtBorders = 1;
    }
    if ((FloatsEqual(y1, 0.0) && FloatsEqual(y2, 1.0)) ||
            (FloatsEqual(y1, 1.0) && FloatsEqual(y2, 0.0))) {
        yAtBorders = 1;
    }
    
    /* Line is diagonal in square. */
    if (xAtBorders && yAtBorders) {
        return kPathGradientPadNone;
    }

    /* From one side to another, parallell to either axes. */
    if (xAtBorders && FloatsEqual(y1, y2)) {
        return kPathGradientPadNone;
    }
    if (yAtBorders && FloatsEqual(x1, x2)) {
        return kPathGradientPadNone;
    }
    
    /* Pad first? */
    if (((x1 > 0.0) && (x2 > x1)) || ((x1 < 1.0) && (x2 < x1))) {
        padFirst = 1;
    }
    if (((y1 > 0.0) && (y2 > y1)) || ((y1 < 1.0) && (y2 < y1))) {
        padFirst = 1;
    }
    
    /* Pad second? */
    if (((x2 < 1.0) && (x2 > x1)) || ((x2 > 0.0) && (x2 < x1))) {
        padSecond = 1;
    }
    if (((y2 < 1.0) && (y2 > y1)) || ((y2 > 0.0) && (y2 < y1))) {
        padSecond = 1;
    }
    if (padFirst && padSecond) {
        return kPathGradientPadBoth;
    } else if (padFirst) {
        return kPathGradientPadFirst;
    } else if (padSecond) {
        return kPathGradientPadSecond;
    } else {
        return kPathGradientPadNone;
    }
}

void
TkPathPaintLinearGradient2(TkPathContext ctx, PathRect *bbox, LinearGradientFill *fillPtr, int fillRule)
{
    int i;
    int pad;
    int nstops;
    PathRect line, transition;
    GradientStop *stop1, *stop2;

    transition = fillPtr->transition;
    nstops = fillPtr->nstops;
    /*
    if (fillRule == WindingRule) {
        CGContextClip(gPathCGContext);
    } else if (fillRule == EvenOddRule) {
        CGContextEOClip(gPathCGContext);
    }
*/
    if (fillPtr->method == kPathGradientMethodPad) {
    
        if ((pad = NeedTransitionPadding(&transition)) != kPathGradientPadNone) {
        
            /* 
             * Construct the pad lines using worst case (diagonal; sqrt(2)):
             * If r1 and r2 are begin and end points of the transition vector,
             * then first pad line is {r1 - sqrt(2)*(r2 - r1), r1}. 
             * Second pad line is {r2, (r2 + sqrt(2)*(r2 - r1)}
             */
            if ((pad == kPathGradientPadFirst) || (pad == kPathGradientPadBoth)) {
                line.x1 = transition.x1 - 1.42*(transition.x2 - transition.x1);
                line.y1 = transition.y1 - 1.42*(transition.y2 - transition.y1);
                line.x2 = transition.x1;
                line.y2 = transition.y1;
                stop1 = fillPtr->stops[0];
                FillTwoStopLinearGradient(d, bbox, &line,
                        stop1->color, stop1->color, stop1->opacity, stop1->opacity);
            }
            if ((pad == kPathGradientPadSecond) || (pad == kPathGradientPadBoth)) {
                line.x1 = transition.x2;
                line.y1 = transition.y2;
                line.x2 = transition.x2 + 1.42*(transition.x2 - transition.x1);
                line.y2 = transition.y2 + 1.42*(transition.y2 - transition.y1);
                stop1 = fillPtr->stops[nstops - 1];
                FillTwoStopLinearGradient(d, bbox, &line,
                        stop1->color, stop1->color, stop1->opacity, stop1->opacity);
            }
        }
        
        /*
         * What happens if first offset > 0.0 or last offset < 1.0? Pad.
         */
        if (fillPtr->stops[0]->offset > 0.0) {
            stop1 = fillPtr->stops[0];
            line.x1 = transition.x1;
            line.y1 = transition.y1;
            line.x2 = transition.x1 + stop1->offset * (transition.x2 - transition.x1);
            line.y2 = transition.y1 + stop1->offset * (transition.y2 - transition.y1);            
            FillTwoStopLinearGradient(d, bbox, &line,
                    stop1->color, stop1->color, stop1->opacity, stop1->opacity);
        }
        if (fillPtr->stops[nstops-1]->offset < 1.0) {
            stop2 = fillPtr->stops[nstops-1];
            line.x1 = transition.x1 + stop2->offset * (transition.x2 - transition.x1);
            line.y1 = transition.y1 + stop2->offset * (transition.y2 - transition.y1);            
            line.x2 = transition.x2;
            line.y2 = transition.y2;
            FillTwoStopLinearGradient(d, bbox, &line,
                    stop2->color, stop2->color, stop2->opacity, stop2->opacity);
        }
        
        /*
         * Paint all stops pairwise.
         */
        for (i = 0; i < nstops - 1; i++) {
            stop1 = fillPtr->stops[i];
            stop2 = fillPtr->stops[i+1];
            
            /* If the two offsets identical then skip. */
            if (fabs(stop1->offset - stop2->offset) < 1e-6) {
                continue;
            }
            
            /* Construct the gradient 'line' by scaling the transition
            * using the stop offsets. */
            line.x1 = transition.x1 + stop1->offset * (transition.x2 - transition.x1);
            line.y1 = transition.y1 + stop1->offset * (transition.y2 - transition.y1);
            line.x2 = transition.x1 + stop2->offset * (transition.x2 - transition.x1);
            line.y2 = transition.y1 + stop2->offset * (transition.y2 - transition.y1);            
            FillTwoStopLinearGradient(d, bbox, &line,
                    stop1->color, stop2->color, stop1->opacity, stop2->opacity);
        }
    } else if (fillPtr->method == kPathGradientMethodRepeat) {
    
        /* @@@ TODO */
    } else if (fillPtr->method == kPathGradientMethodReflect) {
    
        /* @@@ TODO */
    }
}

#endif
