LOCAL_PATH := $(call my-dir)

###########################
#
# tkpath shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := tkpath

tcl_path := $(LOCAL_PATH)/../tcl

include $(tcl_path)/tcl-config.mk

LOCAL_ADDITIONAL_DEPENDENCIES += $(tcl_path)/tcl-config.mk

tk_path := $(LOCAL_PATH)/../sdl2tk

include $(tk_path)/tk-config.mk

LOCAL_ADDITIONAL_DEPENDENCIES += $(tk_path)/tk-config.mk

LOCAL_C_INCLUDES := $(tcl_includes) $(tk_includes) $(LOCAL_PATH)/generic \
	$(tk_path)/sdl/agg-2.4/include $(tk_path)/sdl/agg-2.4/agg2d \
	$(tk_path)/sdl/agg-2.4/font_freetype

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES :=  \
	generic/path.c \
	generic/tkCanvArrow.c \
	generic/tkCanvEllipse.c \
	generic/tkCanvGradient.c \
	generic/tkCanvGroup.c \
	generic/tkCanvPath.c \
	generic/tkCanvPathUtil.c \
	generic/tkCanvPimage.c \
	generic/tkCanvPline.c \
	generic/tkCanvPpoly.c \
	generic/tkCanvPrect.c \
	generic/tkCanvPtext.c \
	generic/tkCanvStyle.c \
	generic/tkPath.c \
	generic/tkPathGradient.c \
	generic/tkPathStyle.c \
	generic/tkPathSurface.c \
	generic/tkPathUtil.c \
	generic/tkpCanvArc.c \
	generic/tkpCanvas.c \
	generic/tkpCanvBmap.c \
	generic/tkpCanvImg.c \
	generic/tkpCanvLine.c \
	generic/tkpCanvPoly.c \
	generic/tkpCanvPs.c \
	generic/tkpCanvText.c \
	generic/tkpCanvUtil.c \
	generic/tkpCanvWind.c \
	generic/tkpRectOval.c \
	generic/tkpTrig.c \
	generic/tkpUtil.c \
	sdl/tkSDLAGGPath.cpp

LOCAL_CFLAGS := $(tcl_cflags) $(tk_cflags) \
	-DPACKAGE_NAME="\"tkpath\"" \
	-DPACKAGE_VERSION="\"0.3.3\"" \
	-O2 -DTKP_NO_POSTSCRIPT=1

LOCAL_SHARED_LIBRARIES := libtcl libtk

LOCAL_LDLIBS :=

include $(BUILD_SHARED_LIBRARY)
