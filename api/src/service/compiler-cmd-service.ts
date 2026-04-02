import type {
  Action,
  CatterConfig,
  CommandCaptureResult,
  ExecutionEvent,
} from "catter-c";

import { CompilerAnalysis } from "../cmd/index.js";
import { IgnorableService, type IgnorableAction } from "./ignorable.js";

/**
 * `IgnorableService` specialization that only forwards top-level compiler
 * driver commands to subclasses.
 *
 * Non-compiler commands are ignored, and compiler descendants such as `cc1`
 * or `collect2` are suppressed automatically once their parent compiler driver
 * command has been seen.
 */
export abstract class CompilerService extends IgnorableService {
  private readonly compilerCommandIds = new Set<number>();
  private readonly compilerServiceAdapter = {
    onStart: (config: CatterConfig): CatterConfig => this.onStart(config),
    onFinish: (event: ExecutionEvent): void => {
      this.onFinish(event);
    },
    onCommand: (id: number, data: CommandCaptureResult): Action => {
      this.rememberCommand(id, data.success ? data.data.parent : undefined);

      if (this.hasIgnoredAncestor(id)) {
        return { type: "skip" };
      }

      if (!data.success || !CompilerAnalysis.supports(data.data.argv)) {
        return { type: "skip" };
      }

      this.compilerCommandIds.add(id);
      const action = this.onCommand(id, data);
      if (action.type === "ignore") {
        return { type: "skip" };
      }

      return action;
    },
    onExecution: (id: number, event: ExecutionEvent): void => {
      if (this.hasIgnoredAncestor(id)) {
        return;
      }

      if (!this.compilerCommandIds.has(id)) {
        return;
      }

      this.onExecution(id, event);
    },
  };

  override asService(): {
    onStart: (config: CatterConfig) => CatterConfig;
    onFinish: (event: ExecutionEvent) => void;
    onCommand: (id: number, data: CommandCaptureResult) => Action;
    onExecution: (id: number, event: ExecutionEvent) => void;
  } {
    return this.compilerServiceAdapter;
  }

  protected override isIgnored(id: number): boolean {
    return this.compilerCommandIds.has(id) || super.isIgnored(id);
  }

  protected override hasIgnoredAncestor(id: number): boolean {
    if (!this.hasCommand(id)) {
      return false;
    }

    let parentId = this.commandParentId(id);
    while (parentId !== undefined) {
      if (this.isIgnored(parentId)) {
        return true;
      }
      parentId = this.commandParentId(parentId);
    }

    return false;
  }

  protected compilerCommandData(
    data: CommandCaptureResult,
  ): CommandCaptureResult & { success: true } {
    if (!data.success || !CompilerAnalysis.supports(data.data.argv)) {
      throw new Error("compiler command data required");
    }

    return data;
  }
}
