.SUFFIXES:
.PHONY:    e1c,e2,clean

compilador := g++ -std=c++11
flagsc     := -Wall

e1c: ejecutivo1-compr_exe
	./$<

e2: ejecutivo2_exe
	./$<

%_exe: %.cpp
	$(compilador) $(flagsc) -o $@ $<

clean:
	rm -rf *_exe *.dSYM
