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

type Event =
    | { kind: "fork"; parent: number; child: number }
    | { kind: "exec"; pid: number; ppid: number; exe: string; args: string[]; cwd: string };

/// What a process runs: an image, or, before it execs, what process `from`
/// ran when it forked it.
type Running = { image: number } | { from: number };

/// The images of a recorder's forks and execs, in the order it logged them.
/// A process's own events come in order, so a fork shows what its parent
/// ran then; a child's exec may come before the fork that made it (auditd
/// logs each system call as it returns, on whichever CPU), so an exec's
/// parent is resolved once every fork is read. An exec whose fork never
/// shows takes what its parent process ran at the exec.
export class ImageBuilder {
    private readonly events: Event[] = [];

    fork(parent: number, child: number): void {
        this.events.push({ kind: "fork", parent, child });
    }

    /// An exec of `pid`, whose parent process was `ppid` when it exec'd.
    exec(pid: number, ppid: number, exe: string, args: string[], cwd: string): void {
        this.events.push({ kind: "exec", pid, ppid, exe, args, cwd });
    }

    build(): Image[] {
        const current = new Map<number, number>();
        const forked = new Map<number, Running>();
        const running = (pid: number): Running => {
            const image = current.get(pid);
            return image === undefined ? { from: pid } : { image };
        };
        const execs: { image: Image; parent: Running; fallback: Running }[] = [];
        for (const event of this.events) {
            if (event.kind === "fork") {
                if (!forked.has(event.child)) {
                    forked.set(event.child, running(event.parent));
                }
                continue;
            }
            const image: Image = {
                id: execs.length,
                parent: null,
                pid: event.pid,
                exe: event.exe,
                args: event.args,
                cwd: event.cwd,
            };
            execs.push({ image, parent: running(event.pid), fallback: running(event.ppid) });
            current.set(event.pid, image.id);
        }
        const resolve = (start: Running, fallback: Running): number | null => {
            const seen = new Set<number>();
            for (let at: Running | undefined = start; at !== undefined;) {
                if ("image" in at) {
                    return at.image;
                }
                if (seen.has(at.from)) {
                    return null;
                }
                seen.add(at.from);
                const next = forked.get(at.from);
                if (next === undefined && at === start && start !== fallback) {
                    at = fallback;
                } else {
                    at = next;
                }
            }
            return null;
        };
        return execs.map(({ image, parent, fallback }) => ({
            ...image,
            parent: resolve(parent, fallback),
        }));
    }
}
