# The first compile fails, which ends the session with
# --abort-on-command-failure; the build runs on, and make, told to keep going
# (-k), runs the second.
all: broken.o fine.o

broken.o: broken.c
	cc -c broken.c -o broken.o

fine.o: fine.c
	cc -c fine.c -o fine.o
