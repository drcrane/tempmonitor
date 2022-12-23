reset
set datafile separator ","
# f(x)=m*x+c
# fit f(x) "lmt86.csv" using 1:2 via m, c
# g(x)=n*x+d
# fit g(x) "lmt86.csv" using 2:1 via n, d
# plot "lmt86.csv" using 1:2 with points, f(x) with lines

# plot "lmt86.csv" using 2:1 with points, g(x) with lines

f(x)=m*x+c
fit f(x) "lmt86.csv" using 3:1 via m, c
plot "lmt86.csv" using 3:1 with points, f(x) with lines
