import {
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

class TestCompilerService extends service.CompilerService {
  readonly seenCommandIds: number[] = [];
  readonly seenExecutionIds: number[] = [];

  override onCommand(
    id: number,
    data: CommandCaptureResult,
  ): service.IgnorableAction {
    debug.assertThrow(data.success);
    this.seenCommandIds.push(id);
    return { type: "skip" };
  }

  override onExecution(id: number, _event: ExecutionEvent): void {
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

const compilerService = new TestCompilerService();
const serviceView = compilerService.asService();

expectSkip(serviceView.onCommand(1, command("make")));
expectSkip(serviceView.onCommand(2, command("gcc", 1)));
expectSkip(serviceView.onCommand(3, command("cc1", 2)));
expectSkip(serviceView.onCommand(4, command("as", 3)));
expectSkip(serviceView.onCommand(5, command("python", 1)));
expectSkip(serviceView.onCommand(6, command("clang", 1)));
expectSkip(serviceView.onCommand(7, command("collect2", 6)));

debug.assertThrow(compilerService.hasCommand(1));
debug.assertThrow(compilerService.hasCommand(7));
debug.assertThrow(compilerService.parentId(7) === 6);
debug.assertThrow(compilerService.ignored(2));
debug.assertThrow(compilerService.ignored(6));
debug.assertThrow(compilerService.ignoredByAncestor(3));
debug.assertThrow(compilerService.ignoredByAncestor(4));
debug.assertThrow(!compilerService.ignoredByAncestor(5));
debug.assertThrow(compilerService.ignoredByAncestor(7));
debug.assertThrow(compilerService.seenCommandIds.length === 2);
debug.assertThrow(compilerService.seenCommandIds[0] === 2);
debug.assertThrow(compilerService.seenCommandIds[1] === 6);

serviceView.onExecution(1, { type: "finish", code: 0 });
serviceView.onExecution(2, { type: "finish", code: 0 });
serviceView.onExecution(3, { type: "finish", code: 0 });
serviceView.onExecution(5, { type: "finish", code: 0 });
serviceView.onExecution(6, { type: "finish", code: 0 });
serviceView.onExecution(7, { type: "finish", code: 0 });
debug.assertThrow(compilerService.seenExecutionIds.length === 2);
debug.assertThrow(compilerService.seenExecutionIds[0] === 2);
debug.assertThrow(compilerService.seenExecutionIds[1] === 6);

function expectSkip(action: { type: string }): void {
  debug.assertThrow(action.type === "skip");
}
