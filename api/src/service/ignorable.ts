import type {
  Action,
  CatterConfig,
  CommandCaptureResult,
  ExecutionEvent,
} from "catter-c";

import { FlatTree } from "../data/index.js";

type TreeState<Id extends PropertyKey, Content> = {
  dataPool: Map<Id, { parent: Id[]; children: Id[]; content: Content }>;
  mergeNode: (node: {
    id: Id;
    content: Content;
    parent?: Id[];
    children?: Id[];
  }) => void;
};

function treeState<Id extends PropertyKey, Content>(
  tree: FlatTree<Id, Content>,
): TreeState<Id, Content> {
  return tree as unknown as TreeState<Id, Content>;
}

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
      this.treeMerge(
        data.success
          ? {
              id,
              parent: data.data.parent === undefined ? [] : [data.data.parent],
              content: data,
            }
          : {
              id,
              content: data,
            },
      );

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
      if (this.treeHas(id) && this.hasIgnoredAncestor(id)) {
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

  protected treeSize(): number {
    return treeState(this.commandTreeState).dataPool.size;
  }

  protected treeContent(id: number): CommandCaptureResult | undefined {
    return treeState(this.commandTreeState).dataPool.get(id)?.content;
  }

  protected treeHas(id: number): boolean {
    return treeState(this.commandTreeState).dataPool.has(id);
  }

  protected treeParentId(id: number): number | undefined {
    const state = treeState(this.commandTreeState);
    const node = state.dataPool.get(id);
    if (node === undefined) {
      return undefined;
    }

    for (const parentId of node.parent) {
      if (state.dataPool.has(parentId)) {
        return parentId;
      }
    }

    return undefined;
  }

  protected treeMerge(node: {
    id: number;
    content: CommandCaptureResult;
    parent?: number[];
    children?: number[];
  }): void {
    treeState(this.commandTreeState).mergeNode(node);
  }

  protected isIgnored(id: number): boolean {
    return this.ignoredCommandIds.has(id);
  }

  protected hasIgnoredAncestor(id: number): boolean {
    if (!this.treeHas(id)) {
      return false;
    }

    let parentId = this.treeParentId(id);
    while (parentId !== undefined) {
      if (this.ignoredCommandIds.has(parentId)) {
        return true;
      }
      parentId = this.treeParentId(parentId);
    }

    return false;
  }
}
