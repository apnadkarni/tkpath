package require tkpath 0.3.0

set t .c_transforms
destroy $t
toplevel $t
set w $t.c
pack [tkp::canvas $w -bg white -width 480 -height 300]

set mskewx1 [::tkp::matrix skewx 0.3]
set mskewx2 [::tkp::matrix skewx 0.5]
set mrot    [::tkp::matrix rotate [expr 3.1415/4] 100 100]

set g1 [$w gradient create linear -stops {{0 lightblue} {1 blue}}]
$w create path "M 10 10 h 200 v 50 h -200 z" -fill $g1 -matrix $mskewx1

set g2 [$w gradient create linear -stops {{0 #f60} {1 #ff6}}]
$w create path "M 10 70 h 200 v 50 h -200 z" -fill $g2 -matrix $mrot

set g4 [$w gradient create linear -stops {{0 white} {0.5 black} {1 white}}]
$w create path "M 10 220 h 200 v 50 h -200 z" -fill $g4 -matrix $mskewx2

