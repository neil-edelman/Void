set term postscript eps enhanced size 20cm, 10cm
set output "Collision.eps"
set multiplot layout 1, 2
set parametric
set colorbox user horizontal origin 0.32,0.385 size 0.18,0.035 front
set palette defined (1 "#00FF00", 2 "#0000FF")
set zlabel "t"
set vrange [0:1]
set xrange [0:128.000000]
set yrange [0:128.000000]
p_x(t) = 2.261007 + (51.355160 - 2.261007)*t
p_y(t) = 112.740845 + (22.188513 - 112.740845)*t
q_x(t) = 58.323818 + (23.001446 - 58.323818)*t
q_y(t) = 24.433094 + (25.301245 - 24.433094)*t
splot p_x(v), p_y(v), v title "p(t) (2.3, 112.7)->(51.4, 22.2) radius 16" lt palette, q_x(v), q_y(v), v title "q(t) (58.3, 24.4)->(23.0, 25.3) radius 16" lt palette lw 10
set xlabel "t"
set ylabel "d"
set xrange [0 : 1]
set yrange [-1 : *]
set label "d_{min} = 18.719498\nt_{min} = 0.827040\nt_0 = 0.618468" at graph 0.05, graph 0.10
set arrow 1 from graph 0, first 32.000000 to graph 1, first 32.000000 nohead lc rgb "blue" lw 3
set arrow 2 from first 0.827040, graph 0 to first 0.827040, graph 1 nohead
set arrow 3 from first 0.618468, graph 0 to first 0.618468, graph 1 nohead
plot "Collision.data" using 1:2 title "Distance |q(t) - p(t)|" with linespoints
