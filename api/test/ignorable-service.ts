import {
  data,
  debug,
  service,
  type CatterRuntime,
  type CommandCaptureResult,
  type ExecutionEvent,
} from "catter";

const runtime: CatterRuntime = {
  supportActions: ["skip", "drop", "abort", "modify"],
  supportEvents: ["finish"],
  type: "inject",
  supportParentId: true,
};

function command(exe: string, parent?: number): CommandCaptureResult {
  return {
    success: true,
    data: {
      cwd: "/tmp",
      exe,
      argv: [exe],
      env: [],
      runtime,
      parent,
    },
  };
}

class TestIgnorableService extends service.IgnorableService {
  readonly seenCommandIds: number[] = [];
  readonly seenExecutionIds: number[] = [];

  override onCommand(
    id: number,
    data: CommandCaptureResult,
  ): service.IgnorableAction {
    this.seenCommandIds.push(id);

    if (data.success && data.data.exe === "gcc") {
      return { type: "ignore" };
    }

    return { type: "skip" };
  }

  override onExecution(id: number, event: ExecutionEvent): void {
    this.seenExecutionIds.push(id);
  }

  relation(left: number, right: number): data.FlatTreeRelation {
    return this.commandTree().relation(left, right);
  }

  treeSize(): number {
    return this.commandTree().size;
  }

  ignored(id: number): boolean {
    return this.isIgnored(id);
  }

  ignoredByAncestor(id: number): boolean {
    return this.hasIgnoredAncestor(id);
  }
}

const ignorable = new TestIgnorableService();
const serviceView = ignorable.asService();

expectSkip(serviceView.onCommand(1, command("make")));
expectSkip(serviceView.onCommand(2, command("gcc", 1)));
expectSkip(serviceView.onCommand(3, command("cc1", 2)));
expectSkip(serviceView.onCommand(4, command("as", 3)));

debug.assertThrow(ignorable.treeSize() === 4);
debug.assertThrow(ignorable.ignored(2));
debug.assertThrow(ignorable.ignoredByAncestor(3));
debug.assertThrow(ignorable.ignoredByAncestor(4));
debug.assertThrow(ignorable.relation(2, 4) === data.FlatTreeRelation.Ancestor);
debug.assertThrow(ignorable.seenCommandIds.length === 2);
debug.assertThrow(ignorable.seenCommandIds[0] === 1);
debug.assertThrow(ignorable.seenCommandIds[1] === 2);

serviceView.onExecution(2, { type: "finish", code: 0 });
serviceView.onExecution(3, { type: "finish", code: 0 });
serviceView.onExecution(4, { type: "finish", code: 0 });
debug.assertThrow(ignorable.seenExecutionIds.length === 1);
debug.assertThrow(ignorable.seenExecutionIds[0] === 2);

function expectSkip(action: service.IgnorableAction): void {
  debug.assertThrow(action.type === "skip");
}
