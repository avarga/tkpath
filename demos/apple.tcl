#package require tkpath
 load /Users/matben/C/cvs/tkpath/macosx/build/tkpath0.1.dylib

set t .c_apple
toplevel $t
set w $t.c
pack [canvas $w -width 400 -height 400 -bg white]

set ::tkpath::antialias 1

set grad [tkpath::lineargradient create  \
  -stops {{0 red} {0.2 yellow} {0.4 green} {0.6 blue}} \
  -transition {0 0 0 1}]
$w create path "M 0 0 c 20 0 40 -20 70 -20 S 130 40 130 80 \
  130 200 70 200   20 180 0 180 z" -fillgradient $grad

$w move all 200 120




