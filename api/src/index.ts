// now it is a demo
export interface Command {
  argv: string[];
  exe: string;
  cwd: string;
  env: Record<string, string>;
  pid: number;
  tid: number;
  raw: string;
}

export type CommandAction =
  | { action: "pass" }
  | { action: "use" }
  | { action: "replace"; commands: string[][] }
  | { action: "drop" };

export interface CatterHandler {
  receiveArgs?(args: string[]): void;
  onStart?(): void;

  onCommand?(cmd: Command): CommandAction;

  onError?(err: { type: number; msg: string; command?: Command }): void;

  onFinished?(): void;
}

export const Catter = {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  register: (handler: CatterHandler) => void {},
};

// compose multiple handler
export class SequenceHandler implements CatterHandler {
  handlers: CatterHandler[];
  constructor(...handlers: CatterHandler[]) {
    this.handlers = handlers;
  }
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  receiveArgs(args: string[]): void {}
  onStart(): void {
    for (const handler of this.handlers) {
      handler.onStart?.();
    }
  }
  onCommand(cmd: Command): CommandAction {
    const currentCmd = cmd;
    for (const handler of this.handlers) {
      const action = handler.onCommand?.(currentCmd);
      if (action) {
        // if some handler returns replace, but another return "drop" / "use" / "replace"
        // if some handler returns "drop". but another return "use" / "replace"
        // if some handler returns "use". but ...
        // we panic!
        // other wise, it's ok.
      }
      return action!;
    }
    return { action: "pass" };
  }
  onFinished(): void {
    for (const handler of this.handlers) {
      handler.onFinished?.();
    }
  }
}
