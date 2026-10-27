# make runs `echo` itself, and the other recipe with /bin/sh -c, which runs
# bash in its place: the command bash runs is a child of both.
all:
	@echo built
	@cd src && ./check.sh one two
