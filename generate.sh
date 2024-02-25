gcc -O3 plotter.c -o ./plotter

mkdir plots

. ./graphs

./plotter plots/heart.bmp "$GRAPH_HEART"
./plotter plots/circle.bmp "$GRAPH_CIRCLE"
./plotter plots/elipses.bmp "$GRAPH_ELIPSES"
./plotter plots/loop.bmp "$GRAPH_LOOP"
./plotter plots/infinity.bmp "$GRAPH_INFINITY"
./plotter plots/trig.bmp "(sin x)" "(cos x)" "(tan x)"