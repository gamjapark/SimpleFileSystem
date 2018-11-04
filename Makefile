HW2: fs.o mount.o disk.o testcase.o
	gcc -o HW2 fs.o mount.o disk.o testcase.o
fs.o: fs.c fs.h disk.h
	gcc -c fs.c
mount.o: mount.c fs.h disk.h
	gcc -c mount.c
disk.o: disk.c disk.h
	gcc -c disk.c
testcase.o: testcase.c fs.h disk.h
	gcc -c testcase.c
clean:
	rm -f mount.o fs.o disk.o testcase.o


