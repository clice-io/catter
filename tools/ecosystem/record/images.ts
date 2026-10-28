/// What a recorder saw: one image per successful exec, each with the image
/// that started it. An exec's parent is the image its process ran when it
/// called exec, or, for a process that has not exec'd yet, the image its
/// parent ran when it forked it: catter's model of a command's parent.

export interface Image {
    id: number;
    parent: number | null;
    pid: number;
    exe: string;
    args: string[];
    cwd: string;
}

export interface Recording {
    images: Image[];
    /// Events the recorder says it lost; a recording that lost any proves
    /// nothing about a command it lacks.
    lost: number;
}

interface Exec {
    pid: number;
    ppid: number;
    exe: string;
    args: string[];
    cwd: string;
}

/// The images of a recorder's forks and execs, in the order it logged them.
/// A recorder may log a fork after what the child did next (auditd logs each
/// system call as it returns, on whichever CPU), so the parents are resolved
/// once every fork is known: an exec's process is followed up through the
/// processes that forked it, past those that never exec'd (a subshell, the
/// middle of a daemon's double fork), to the nearest that ran an image.
export class ImageBuilder {
    private readonly forkedBy = new Map<number, number>();
    private readonly execs: Exec[] = [];

    fork(parent: number, child: number): void {
        if (!this.forkedBy.has(child)) {
            this.forkedBy.set(child, parent);
        }
    }

    /// An exec of `pid`, whose parent process was `ppid` when it exec'd.
    exec(pid: number, ppid: number, exe: string, args: string[], cwd: string): void {
        this.execs.push({ pid, ppid, exe, args, cwd });
    }

    build(): Image[] {
        const images: Image[] = [];
        const current = new Map<number, number>();
        for (const exec of this.execs) {
            let parent = current.get(exec.pid);
            if (parent === undefined) {
                const seen = new Set<number>();
                for (
                    let at: number | undefined = this.forkedBy.get(exec.pid) ?? exec.ppid;
                    at !== undefined && !seen.has(at);
                    at = this.forkedBy.get(at)
                ) {
                    seen.add(at);
                    parent = current.get(at);
                    if (parent !== undefined) {
                        break;
                    }
                }
            }
            const image: Image = {
                id: images.length,
                parent: parent ?? null,
                pid: exec.pid,
                exe: exec.exe,
                args: exec.args,
                cwd: exec.cwd,
            };
            images.push(image);
            current.set(exec.pid, image.id);
        }
        return images;
    }
}
