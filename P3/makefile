.SUFFIXES:
.PHONY:    pc,f,fi,fc,clean

compilador := mpicxx -std=c++11
flagsc     := -g -Wall

pc: prodcons_varios_exe
	mpirun -np 10 ./$<

pcm: prodcons_varios_modos_exe
	mpirun -np 10 ./$<

fp: filosofos-plantilla_exe
	mpirun -np 10 ./$<

fi: filosofos-interb_exe
	mpirun -np 10 ./$<

f: filosofos_exe
	mpirun -np 10 ./$<

fc: filosofos-cam_exe
	mpirun -np 11 ./$<

%_exe: %.cpp
	$(compilador) $(flagsc) -o $@ $<

clean:
	rm -rf *_exe *.dSYM
