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

export class ImageBuilder {
    readonly images: Image[] = [];
    private readonly current = new Map<number, number | null>();
    /// Processes whose exec came before the fork that made them, which the
    /// recorder may log in either order: the fork, when it comes, must not
    /// take back what the exec set.
    private readonly early = new Set<number>();

    fork(parent: number, child: number): void {
        if (this.early.delete(child)) {
            return;
        }
        this.current.set(child, this.current.get(parent) ?? null);
    }

    /// An exec of `pid`, whose parent process at the time was `ppid`.
    exec(pid: number, ppid: number, exe: string, args: string[], cwd: string): Image {
        let parent = this.current.get(pid);
        if (parent === undefined) {
            this.early.add(pid);
            parent = this.current.get(ppid) ?? null;
        }
        const image: Image = { id: this.images.length, parent, pid, exe, args, cwd };
        this.images.push(image);
        this.current.set(pid, image.id);
        return image;
    }
}
