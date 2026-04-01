import * as io from "../io.js";

/**
 * A comparable node identifier supported by `FlatTree`.
 */
export type FlatTreeId = PropertyKey;

/**
 * A single flat node description used to build or update a `FlatTree`.
 */
export interface FlatTreeNodeInput<Id extends FlatTreeId, Content> {
  id: Id;
  content: Content;
  parent?: Id[];
  children?: Id[];
}

export interface FlatTreeNodeStore<Id extends FlatTreeId, Content> {
  id: Id;
  content: Content;
  parent: Id[];
  children: Id[];
}

/**
 * Relative relationship between two ids in a `FlatTree`.
 */
export const FlatTreeRelation = {
  Self: "self",
  Ancestor: "ancestor",
  Descendant: "descendant",
  None: "none",
} as const;
export type FlatTreeRelation =
  (typeof FlatTreeRelation)[keyof typeof FlatTreeRelation];

export interface FlatTreeWalker<Id extends FlatTreeId> {
  first: Id | undefined;
  children(id: Id | undefined): readonly Id[];
}

/**
 * A mutable tree view built from flat `(id, parentId, content)` items.
 *
 * Missing parents are treated as roots. Nodes may be added incrementally,
 * parents may appear after children, and a node may be referenced by multiple
 * parents, forming a DAG.
 */

export class FlatTree<Id extends FlatTreeId, Content> {
  private dataPool: Map<Id, FlatTreeNodeStore<Id, Content>> = new Map();

  constructor() {}

  justMergeNode(node: FlatTreeNodeInput<Id, Content>) {
    if (this.dataPool.has(node.id)) {
      const pre = this.dataPool.get(node.id)!;
      pre.parent = Array.from(new Set([...pre.parent, ...(node.parent ?? [])]));
      pre.children = Array.from(
        new Set([...pre.children, ...(node.children ?? [])]),
      );
      pre.content = node.content;
    } else {
      this.dataPool.set(node.id, {
        parent: node.parent ?? [],
        children: node.children ?? [],
        ...node,
      });
    }
  }

  justUpdateNode(node: FlatTreeNodeInput<Id, Content>) {
    this.dataPool.set(node.id, {
      parent: node.parent ?? [],
      children: node.children ?? [],
      ...node,
    });
  }

  private isRoot(id: Id): boolean {
    return (this.dataPool.get(id)?.parent?.length ?? 0) === 0;
  }

  private stitchEdges() {
    for (const [id, node] of this.dataPool) {
      for (const childId of node.children) {
        const childNode = this.dataPool.get(childId);
        if (childNode && !childNode.parent.includes(id)) {
          childNode.parent.push(id);
        }
      }
      for (const parentId of node.parent) {
        const parentNode = this.dataPool.get(parentId);
        if (parentNode && !parentNode.children.includes(id)) {
          parentNode.children.push(id);
        }
      }
    }
  }

  assemble(): boolean {
    this.stitchEdges();

    // 0=unvisited, 1=visited in this tree, 2=visited previously, no cycle found
    const states: Map<Id, 0 | 1 | 2> = new Map();
    for (const id of this.dataPool.keys()) {
      states.set(id, 0);
    }

    let noCycle = true;

    const hasCycleDFS = (nodeId: Id): boolean => {
      const state = states.get(nodeId);
      if (state === 1) return true;
      if (state === 2) return false;

      states.set(nodeId, 1);

      const node = this.dataPool.get(nodeId);
      if (node) {
        for (const childId of node.children) {
          if (hasCycleDFS(childId)) {
            return true;
          }
        }
      }

      states.set(nodeId, 2);
      return false;
    };

    for (const id of this.dataPool.keys()) {
      if (states.get(id) === 0) {
        if (hasCycleDFS(id)) {
          noCycle = false;
          break;
        }
      }
    }

    return noCycle;
  }

  roots(): Id[] {
    this.stitchEdges();
    const res: Id[] = [];

    for (const id of this.dataPool.keys()) {
      if (this.isRoot(id)) {
        res.push(id);
      }
    }

    return res;
  }

  walk(): FlatTreeWalker<Id> {
    const roots = this.roots();

    return {
      first: roots.length === 1 ? roots[0] : undefined,
      children: (id: Id | undefined): readonly Id[] => {
        if (id === undefined) {
          return roots;
        }
        return this.dataPool.get(id)?.children ?? [];
      },
    };
  }

  relation(leftId: Id, rightId: Id): FlatTreeRelation {
    this.stitchEdges();

    if (leftId === rightId) {
      return FlatTreeRelation.Self;
    }

    const reach = (from: Id, to: Id): boolean => {
      const pending = [from];
      const seen = new Set<Id>();

      while (pending.length > 0) {
        const curr = pending.pop() as Id;
        if (seen.has(curr)) {
          continue;
        }
        seen.add(curr);

        const node = this.dataPool.get(curr);
        if (!node) {
          continue;
        }

        for (const childId of node.children) {
          if (childId === to) {
            return true;
          }
          pending.push(childId);
        }
      }

      return false;
    };

    if (reach(leftId, rightId)) {
      return FlatTreeRelation.Ancestor;
    }
    if (reach(rightId, leftId)) {
      return FlatTreeRelation.Descendant;
    }
    return FlatTreeRelation.None;
  }
}
