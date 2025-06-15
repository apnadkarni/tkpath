# all.tcl --
#
# This file contains a top-level script to run all of the Tk
# tests.  Execute it by invoking "source all.tcl" when running tktest
# in this directory.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2015 René Zaumseil <r.zaumseil@freenet.de>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

package require Tcl 8.5
package require tcltest 2
package require Tk ;# This is the Tk test suite; fail early if no Tk!
package require tkpath ;# Our own package

tcltest::configure \
	-testdir [file normalize [file dirname [info script]]] \
	-singleproc 1 \
	{*}$::argv

namespace import ::tcltest::*

proc ::tkp_setup {} {
    catch {destroy {*}[winfo children .]}
    pack [::tkp::canvas .c -width 60 -height 40 -bd 0 -highlightthickness 0]
    update
}

proc ::tkp_cleanup {} {
    cleanupTests
    catch {destroy {*}[winfo children .]}
}

tcltest::runAllTests
