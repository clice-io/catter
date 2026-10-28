# Leaves a job behind, which runs on once the build has ended: it waits for a
# line on the fifo `go`, then writes to the fifo `done` what traces it.
mkfifo go done
(read line < go; grep TracerPid /proc/self/status > done) < /dev/null > /dev/null 2>&1 &
echo started
