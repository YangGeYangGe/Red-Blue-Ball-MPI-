assignment: assignment1.c
	mpicc -o assignment assignment1.c
.PHONY:clean
clean:
	rm assignment