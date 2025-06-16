/*
 * path.c --
 *
 *	This file is main for the tkpath package.
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2005-2008  Mats Bengtsson
 *
 */

#include "tkp.h"
#include "tkIntPath.h"

#ifdef _WIN32
#define NO_STRICT
#include <windows.h>
#endif

/* Keep patch level release numbers odd and set even only on release. */
#define TKPATH_REQUIRE    "9.0"

MODULE_SCOPE int gAntiAlias;
MODULE_SCOPE int gSurfaceCopyPremultiplyAlpha;
MODULE_SCOPE int gDepixelize;
MODULE_SCOPE int gCanZlib;

int gAntiAlias = 1;
int gSurfaceCopyPremultiplyAlpha = 1;
int gDepixelize = 1;
int gCanZlib = 0;

#if defined(_WIN32) && defined(AGG_CUSTOM_ALLOCATOR)
/* AGG custom allocator functions */
void *(*agg_custom_alloc)(unsigned int size) = NULL;
void (*agg_custom_free)(void *ptr) = NULL;
#endif

MODULE_SCOPE int PixelAlignObjCmd(ClientData clientData, Tcl_Interp* interp,
                    int objc, Tcl_Obj* const objv[]);
MODULE_SCOPE int SurfaceInit(Tcl_Interp *interp);


#if defined(_WIN32) && !defined(PLATFORM_SDL)
    BOOL APIENTRY
    DllMain( HINSTANCE hInst,   /* Library instance handle. */
             DWORD     reason,  /* Reason this function is being called. */
             LPVOID    reserved /* Not used. */ )
    {
        return TRUE;
    }
#endif


/*
 *----------------------------------------------------------------------
 *
 * Tkpath_Init --
 *
 *		Initializer for the tkpath package.
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#ifdef __cplusplus
extern "C" {
#endif
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
    __declspec(dllexport)
#endif

int Tkpath_Init(Tcl_Interp *interp)		/* Tcl interpreter. */
{
    const char *tclver;
    int major = -1, minor = -1;

#if defined(USE_TCL_STUBS)
    if (Tcl_InitStubs(interp, TKPATH_REQUIRE, 0) == NULL) {
	return TCL_ERROR;
    }
#endif
#if defined(_WIN32) && defined(AGG_CUSTOM_ALLOCATOR)
    agg_custom_alloc = (void *(*)(unsigned int)) Tcl_Alloc;
    agg_custom_free = (void (*)(void *)) Tcl_Free;
#endif
    tclver = Tcl_PkgRequire(interp, "Tcl", TKPATH_REQUIRE, 0);
    if (tclver == NULL) {
	return TCL_ERROR;
    }
    sscanf(tclver, "%d.%d", &major, &minor);
    if ((major > 8) || ((major == 8) && (minor >= 6))) {
	gCanZlib = 1;
    }
#if defined(USE_TK_STUBS)
    if (Tk_InitStubs(interp, TKPATH_REQUIRE, 0) == NULL) {
	return TCL_ERROR;
    }
#endif
    if (Tcl_PkgRequire(interp, "Tk", TKPATH_REQUIRE, 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_CreateNamespace(interp, "::tkp", NULL, NULL) == NULL) {
	Tcl_ResetResult(interp);
    }
    Tcl_CreateObjCommand(interp, "::tkp::canvas", Tk_PathCanvasObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);

    /*
     * Link the ::tkp::antialias variable to control antialiasing.
     */
    if (Tcl_LinkVar(interp, "::tkp::antialias",
            (char *) &gAntiAlias, TCL_LINK_BOOLEAN) != TCL_OK) {
        Tcl_ResetResult(interp);
    }

    /*
     * With gSurfaceCopyPremultiplyAlpha true we ignore the "premultiply alpha"
     * and use RGB as is. Else we need to divide each RGB with alpha
     * to get "true" values.
     */
    if (Tcl_LinkVar(interp, "::tkp::premultiplyalpha",
            (char *) &gSurfaceCopyPremultiplyAlpha,
	    TCL_LINK_BOOLEAN) != TCL_OK) {
        Tcl_ResetResult(interp);
    }
    if (Tcl_LinkVar(interp, "::tkp::depixelize",
            (char *) &gDepixelize, TCL_LINK_BOOLEAN) != TCL_OK) {
        Tcl_ResetResult(interp);
    }
    Tcl_CreateObjCommand(interp, "::tkp::pixelalign",
            PixelAlignObjCmd, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    /*
     * Make separate gradient objects, similar to SVG.
     */
    PathGradientInit(interp);
    SurfaceInit(interp);

    /*
     * Style object.
     */
    PathStyleInit(interp);

    return Tcl_PkgProvide(interp, "tkpath", PACKAGE_VERSION);
}

/*
 *----------------------------------------------------------------------
 *
 * Tkpath_SafeInit --
 *
 *		This is just to provide a "safe" entry point (that is not safe!).
 *
 * Results:
 *		A standard Tcl result.
 *
 * Side Effects:
 *   	Tcl commands created
 *
 *----------------------------------------------------------------------
 */
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
    __declspec(dllexport)
#endif

int Tkpath_SafeInit(Tcl_Interp *interp)	    { return Tkpath_Init(interp); }
int Tkpath_Unload(Tcl_Interp *interp)	    { return TCL_ERROR; }
int Tkpath_SafeUnload(Tcl_Interp *interp)   { return Tkpath_Unload( interp ); }
#ifdef __cplusplus
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
