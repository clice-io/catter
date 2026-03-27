import type {
  Action,
  CatterConfig,
  CommandCaptureResult,
  ExecutionEvent,
} from "catter-c";

import { FlatTree } from "../data/index.js";

/**
 * Extended command action that suppresses forwarding of all descendant
 * commands to the subclass while still letting the current command execute.
 */
export type IgnoreAction = {
  type: "ignore";
};

/**
 * Command result supported by `IgnorableService` subclasses.
 */
export type IgnorableAction = Action | IgnoreAction;

/**
 * Base service that can ignore a command subtree while still tracking the
 * full command hierarchy in a single mutable `FlatTree`.
 *
 * Subclasses still override the usual `onStart` / `onFinish` / `onCommand` /
 * `onExecution` lifecycle methods. The only semantic extension is that
 * `onCommand` may additionally return `{ type: "ignore" }`.
 */
export abstract class IgnorableService {
  private readonly commandTreeState = new FlatTree<
    number,
    CommandCaptureResult
  >();
  private readonly ignoredCommandIds = new Set<number>();
  private readonly serviceAdapter = {
    onStart: (config: CatterConfig): CatterConfig => this.onStart(config),
    onFinish: (event: ExecutionEvent): void => {
      this.onFinish(event);
    },
    onCommand: (id: number, data: CommandCaptureResult): Action => {
      this.commandTreeState.add({
        id,
        parentId: data.success ? (data.data.parent ?? null) : null,
        content: data,
      });

      if (this.hasIgnoredAncestor(id)) {
        return { type: "skip" };
      }

      const action = this.onCommand(id, data);
      if (action.type === "ignore") {
        this.ignoredCommandIds.add(id);
        return { type: "skip" };
      }

      return action;
    },
    onExecution: (id: number, event: ExecutionEvent): void => {
      if (this.commandTreeState.has(id) && this.hasIgnoredAncestor(id)) {
        return;
      }

      this.onExecution(id, event);
    },
  };

  onStart(config: CatterConfig): CatterConfig {
    return config;
  }

  onFinish(_event: ExecutionEvent): void {}

  onCommand(_id: number, _data: CommandCaptureResult): IgnorableAction {
    return { type: "skip" };
  }

  onExecution(_id: number, _event: ExecutionEvent): void {}

  asService(): {
    onStart: (config: CatterConfig) => CatterConfig;
    onFinish: (event: ExecutionEvent) => void;
    onCommand: (id: number, data: CommandCaptureResult) => Action;
    onExecution: (id: number, event: ExecutionEvent) => void;
  } {
    return this.serviceAdapter;
  }

  protected commandTree(): FlatTree<number, CommandCaptureResult> {
    return this.commandTreeState;
  }

  protected isIgnored(id: number): boolean {
    return this.ignoredCommandIds.has(id);
  }

  protected hasIgnoredAncestor(id: number): boolean {
    if (!this.commandTreeState.has(id)) {
      return false;
    }

    let parent = this.commandTreeState.parent(id);
    while (parent !== undefined) {
      if (this.ignoredCommandIds.has(parent.id)) {
        return true;
      }
      parent = this.commandTreeState.parent(parent.id);
    }

    return false;
  }
}
