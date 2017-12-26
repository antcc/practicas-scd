CXXFLAGS := $(shell libpng-config --cflags)
LDFLAGS := $(shell libpng-config --ldflags)
INCLUDES = -I./include
BIN = bin

run: mandelbrot clean
	mpirun -np 4 $(BIN)/$<

mandelbrot: mandelbrot.o
	mpicxx -std=c++11 -Wall -o bin/$@ $< $(LDFLAGS)

mandelbrot.o: mandelbrot.cpp
	mpicxx -c $(INCLUDES) $(CXXFLAGS) -std=c++11 -Wall $< 

clean:
	rm -rf *.o
