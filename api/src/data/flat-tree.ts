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

/**
 * Lightweight traversal view returned by `FlatTree.walk()`.
 *
 * `first` is populated only when the graph has exactly one start node. When
 * multiple starts exist, callers should begin from `children(undefined)`.
 *
 * @example
 * ```ts
 * import { FlatTree } from "catter/data";
 *
 * const tree = new FlatTree<number, string>();
 * tree.justMergeNode({ id: 1, content: "root" });
 * tree.justMergeNode({ id: 2, parent: [1], content: "leaf" });
 *
 * const walker = tree.walk();
 * console.log(walker.first);
 * console.log(walker.children(undefined));
 * console.log(walker.children(1));
 * ```
 *
 * Output:
 * ```txt
 * 1
 * [1]
 * [2]
 * ```
 */
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
 *
 * @example
 * ```ts
 * import { FlatTree } from "catter/data";
 *
 * const tree = new FlatTree<string, string>();
 * tree.justMergeNode({ id: "app", content: "app" });
 * tree.justMergeNode({ id: "main.o", parent: ["app"], content: "main.o" });
 * tree.justMergeNode({ id: "util.o", parent: ["app"], content: "util.o" });
 *
 * console.log(tree.assemble());
 * console.log(tree.roots());
 * console.log(tree.walk().children("app"));
 * ```
 *
 * Output:
 * ```txt
 * []
 * ["app"]
 * ["main.o", "util.o"]
 * ```
 */

export class FlatTree<Id extends FlatTreeId, Content> {
  private dataPool: Map<Id, FlatTreeNodeStore<Id, Content>> = new Map();

  constructor() {}

  /**
   * Merges one node into the current graph without validating cycles.
   *
   * Existing parent and child links are unioned, while `content` is replaced
   * by the new value.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 2, parent: [1], content: "leaf" });
   * tree.justMergeNode({ id: 2, parent: [3], content: "leaf" });
   *
   * console.log(tree.node(2)?.parent);
   * ```
   *
   * Output:
   * ```txt
   * [1, 3]
   * ```
   */
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
    this.detachNode(node.id);
    this.dataPool.set(node.id, {
      parent: node.parent ?? [],
      children: node.children ?? [],
      ...node,
    });
  }

  justRemoveNode(id: Id) {
    this.detachNode(id);
    this.dataPool.delete(id);
  }

  /**
   * Merges one node into the graph, then reassembles it.
   *
   * See `assemble()` for the returned cycle report.
   */
  merge(node: FlatTreeNodeInput<Id, Content>) {
    this.justMergeNode(node);
    return this.assemble();
  }

  /**
   * Replaces one node's links and content, then reassembles the graph.
   *
   * See `assemble()` for the returned cycle report.
   */
  update(node: FlatTreeNodeInput<Id, Content>) {
    this.justUpdateNode(node);
    return this.assemble();
  }

  /**
   * Removes one node, then reassembles the graph.
   *
   * See `assemble()` for the returned cycle report.
   */
  remove(id: Id) {
    this.justRemoveNode(id);
    return this.assemble();
  }

  isRoot(id: Id): boolean {
    return (this.dataPool.get(id)?.parent?.length ?? 0) === 0;
  }

  isStart(id: Id): boolean {
    const node = this.dataPool.get(id);
    if (!node) {
      return false;
    }

    return (
      node.parent.length === 0 ||
      node.parent.every((parentId) => !this.dataPool.has(parentId))
    );
  }

  private detachNode(id: Id): void {
    const existing = this.dataPool.get(id);
    if (!existing) {
      return;
    }

    for (const parentId of existing.parent) {
      const parentNode = this.dataPool.get(parentId);
      if (parentNode) {
        parentNode.children = parentNode.children.filter(
          (childId) => childId !== id,
        );
      }
    }

    for (const childId of existing.children) {
      const childNode = this.dataPool.get(childId);
      if (childNode) {
        childNode.parent = childNode.parent.filter(
          (parentId) => parentId !== id,
        );
      }
    }
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

  /**
   * Stitches parent/child edges and topologically sorts the graph using
   * Kahn's algorithm to detect cycles.
   *
   * The result is one representative directed cycle per strongly connected
   * cyclic component. Each cycle is an ordered list of node ids where every
   * consecutive pair (and the last back to the first) is an edge; a self-loop
   * is reported as a single-node cycle. The union of all returned ids is
   * exactly the set of nodes that participate in a cycle. An empty array means
   * the graph is acyclic.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 1, children: [2], content: "one" });
   * tree.justMergeNode({ id: 2, children: [1], content: "two" });
   *
   * console.log(tree.assemble());
   * ```
   *
   * Output:
   * ```txt
   * [[1, 2]]
   * ```
   */
  assemble(): readonly (readonly Id[])[] {
    this.stitchEdges();

    // Kahn's algorithm: repeatedly remove nodes whose in-tree parents are all
    // gone. Any node left over is part of a cycle or reachable from one.
    const inDegree = new Map<Id, number>();
    for (const id of this.dataPool.keys()) {
      inDegree.set(id, 0);
    }
    for (const [id, node] of this.dataPool) {
      for (const parentId of node.parent) {
        if (this.dataPool.has(parentId)) {
          inDegree.set(id, (inDegree.get(id) as number) + 1);
        }
      }
    }

    const ready: Id[] = [];
    for (const [id, degree] of inDegree) {
      if (degree === 0) {
        ready.push(id);
      }
    }

    let removed = 0;
    while (ready.length > 0) {
      const id = ready.pop() as Id;
      removed += 1;
      const node = this.dataPool.get(id);
      if (!node) {
        continue;
      }
      for (const childId of node.children) {
        const degree = inDegree.get(childId);
        if (degree === undefined) {
          continue;
        }
        const next = degree - 1;
        inDegree.set(childId, next);
        if (next === 0) {
          ready.push(childId);
        }
      }
    }

    if (removed === this.dataPool.size) {
      return [];
    }

    // Narrow the surviving nodes to those actually in a cycle. Nodes that are
    // only downstream of a cycle (reachable but not strongly connected back)
    // are excluded by keeping just the non-trivial SCCs via Tarjan's algorithm.
    const leftover = new Set<Id>();
    for (const [id, degree] of inDegree) {
      if (degree > 0) {
        leftover.add(id);
      }
    }

    const index = new Map<Id, number>();
    const lowLink = new Map<Id, number>();
    const onStack = new Set<Id>();
    const stack: Id[] = [];
    const cycles: Id[][] = [];
    let nextIndex = 0;

    const strongConnect = (nodeId: Id): void => {
      index.set(nodeId, nextIndex);
      lowLink.set(nodeId, nextIndex);
      nextIndex += 1;
      stack.push(nodeId);
      onStack.add(nodeId);

      const node = this.dataPool.get(nodeId);
      const children = node
        ? node.children.filter((childId) => leftover.has(childId))
        : [];
      for (const childId of children) {
        if (!index.has(childId)) {
          strongConnect(childId);
          lowLink.set(
            nodeId,
            Math.min(
              lowLink.get(nodeId) as number,
              lowLink.get(childId) as number,
            ),
          );
        } else if (onStack.has(childId)) {
          lowLink.set(
            nodeId,
            Math.min(
              lowLink.get(nodeId) as number,
              index.get(childId) as number,
            ),
          );
        }
      }

      if (lowLink.get(nodeId) !== index.get(nodeId)) {
        return;
      }

      const component: Id[] = [];
      let top: Id;
      do {
        top = stack.pop() as Id;
        onStack.delete(top);
        component.push(top);
      } while (top !== nodeId);

      const selfLoop = component.some(
        (memberId) =>
          this.dataPool.get(memberId)?.children.includes(memberId) ?? false,
      );
      if (component.length > 1 || selfLoop) {
        cycles.push(this.extractCycle(component));
      }
    };

    for (const id of leftover) {
      if (!index.has(id)) {
        strongConnect(id);
      }
    }

    return cycles;
  }

  /**
   * Extracts one ordered directed cycle from a strongly connected component.
   *
   * Walks outgoing edges within the component until a node repeats; the
   * repeated segment is a simple cycle. A single-node component is only
   * passed in for self-loops and yields `[id]`.
   */
  private extractCycle(component: Id[]): Id[] {
    const componentSet = new Set(component);
    const position = new Map<Id, number>();
    const path: Id[] = [];
    let current = component[0] as Id;

    while (!position.has(current)) {
      position.set(current, path.length);
      path.push(current);
      const children =
        this.dataPool
          .get(current)
          ?.children.filter((childId) => componentSet.has(childId)) ?? [];
      // Every node in a non-trivial SCC has an outgoing edge that stays inside
      // the component, so the walk is guaranteed to loop back on itself.
      current = children[0] as Id;
    }

    return path.slice(position.get(current) as number);
  }

  /**
   * Returns ids whose parent list is empty.
   *
   * Unlike `starts()`, this does not treat missing parents as entry points.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 1, content: "root" });
   * tree.justMergeNode({ id: 2, parent: [99], content: "detached" });
   *
   * console.log(tree.roots());
   * console.log(tree.starts());
   * ```
   *
   * Output:
   * ```txt
   * [1]
   * [1, 2]
   * ```
   */
  roots(): Id[] {
    this.stitchEdges();
    return Array.from(this.dataPool.keys()).filter((id) => this.isRoot(id));
  }

  starts(): Id[] {
    this.stitchEdges();
    return Array.from(this.dataPool.keys()).filter((id) => this.isStart(id));
  }

  /**
   * Builds a traversal helper over the stitched graph.
   *
   * The virtual root `children(undefined)` returns every start node. This is
   * especially useful when the graph is a forest or when some nodes reference
   * parents that were never inserted.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 2, parent: [1], content: "child" });
   * tree.justMergeNode({ id: 3, content: "orphan" });
   *
   * const walker = tree.walk();
   * console.log(walker.first);
   * console.log(walker.children(undefined));
   * ```
   *
   * Output:
   * ```txt
   * undefined
   * [2, 3]
   * ```
   */
  walk(): FlatTreeWalker<Id> {
    const starts = this.starts();

    return {
      first: starts.length === 1 ? starts[0] : undefined,
      children: (id: Id | undefined): readonly Id[] => {
        if (id === undefined) {
          return starts;
        }
        return this.dataPool.get(id)?.children ?? [];
      },
    };
  }

  /**
   * Reports the relative reachability between two ids.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 1, content: "root" });
   * tree.justMergeNode({ id: 2, parent: [1], content: "child" });
   *
   * console.log(tree.relation(1, 2));
   * console.log(tree.relation(2, 1));
   * console.log(tree.relation(2, 2));
   * ```
   *
   * Output:
   * ```txt
   * ancestor
   * descendant
   * self
   * ```
   */
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

  /**
   * Returns the number of stored nodes.
   *
   * @example
   * ```ts
   * import { FlatTree } from "catter/data";
   *
   * const tree = new FlatTree<number, string>();
   * tree.justMergeNode({ id: 1, content: "root" });
   * tree.justMergeNode({ id: 2, parent: [1], content: "leaf" });
   *
   * console.log(tree.size());
   * ```
   *
   * Output:
   * ```txt
   * 2
   * ```
   */
  size() {
    return this.dataPool.size;
  }

  node(id: Id) {
    return this.dataPool.get(id);
  }

  nodes() {
    return this.dataPool.values();
  }

  reset() {
    this.dataPool.clear();
  }
}
