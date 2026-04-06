import {
  debug,
  service,
  type CatterRuntime,
  type CommandCaptureResult,
  type ProcessResult,
} from "catter";

const runtime: CatterRuntime = {
  supportActions: ["skip", "drop", "abort", "modify"],
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

  override onExecution(id: number, _result: ProcessResult): void {
    this.seenExecutionIds.push(id);
  }

  hasCommand(id: number): boolean {
    return super.hasCommand(id);
  }

  parentId(id: number): number | undefined {
    return super.commandParentId(id);
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

debug.assertThrow(ignorable.hasCommand(1));
debug.assertThrow(ignorable.hasCommand(4));
debug.assertThrow(ignorable.parentId(4) === 3);
debug.assertThrow(ignorable.ignored(2));
debug.assertThrow(ignorable.ignoredByAncestor(3));
debug.assertThrow(ignorable.ignoredByAncestor(4));
debug.assertThrow(ignorable.seenCommandIds.length === 2);
debug.assertThrow(ignorable.seenCommandIds[0] === 1);
debug.assertThrow(ignorable.seenCommandIds[1] === 2);

serviceView.onExecution(2, { code: 0, stdout: "", stderr: "" });
serviceView.onExecution(3, { code: 0, stdout: "", stderr: "" });
serviceView.onExecution(4, { code: 0, stdout: "", stderr: "" });
debug.assertThrow(ignorable.seenExecutionIds.length === 1);
debug.assertThrow(ignorable.seenExecutionIds[0] === 2);

function expectSkip(action: service.IgnorableAction): void {
  debug.assertThrow(action.type === "skip");
}
