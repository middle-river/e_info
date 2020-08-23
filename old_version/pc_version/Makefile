all : e_info_test

clean : 
	rm -f e_info_test *.o

e_info_test : e_info_test.cpp Script.o
	g++ -Wall -o $@ $+

Script.o : ../Script.cpp ../Script.h String.h
	g++ -Wall -c $<
