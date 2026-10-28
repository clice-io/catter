# Leaves a process that its parent's exit hands to another, as a daemon is,
# and has it compile a.c while the build waits for it.
mkfifo go done
( (read line < go; cc -c a.c; echo > done) & )
echo > go
read line < done
