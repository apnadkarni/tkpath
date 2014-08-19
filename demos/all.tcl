
set dir [file dirname [info script]]
set tail [file tail [info script]]
foreach fileName [glob -nocomplain -directory $dir *.tcl] {
    if {[file tail $fileName] ne $tail} {
	source $fileName
    }
}

