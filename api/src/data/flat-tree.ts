import * as io from "../io.js";

/**
 * A comparable node identifier supported by `FlatTree`.
 */
export type FlatTreeId = PropertyKey;

/**
 * A single flat node description used to build or update a `FlatTree`.
 */
export interface FlatTreeItem<Id extends FlatTreeId, Content> {
  id: Id;
  parentId?: Id | null;
  content: Content;
}

/**
 * A read-only node exposed by `FlatTree`.
 */
export interface FlatTreeNode<Id extends FlatTreeId, Content> {
  readonly id: Id;
  readonly parentId: Id | null;
  readonly content: Content;
  readonly children: readonly FlatTreeNode<Id, Content>[];
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
 * Options for rendering a `FlatTree` as text.
 */
export interface FlatTreeRenderOptions<Id extends FlatTreeId, Content> {
  /**
   * Converts node content into display text.
   *
   * By default, object-like values are JSON stringified and primitive values
   * use `String(value)`.
   */
  stringify?: (content: Content, node: FlatTreeNode<Id, Content>) => string;

  /**
   * @deprecated Repeated subtree collapsing is no longer applied.
   */
  collapseRepeated?: boolean;

  /**
   * @deprecated Repeated subtree collapsing is no longer applied.
   */
  contentEquals?: (left: Content, right: Content) => boolean;

  /**
   * Limits render depth, where `0` means only roots are printed.
   *
   * When omitted, the whole tree is rendered.
   */
  maxDepth?: number;
}

type MutableFlatTreeNode<Id extends FlatTreeId, Content> = {
  id: Id;
  parentId: Id | null;
  content: Content;
  children: MutableFlatTreeNode<Id, Content>[];
};

const TREE_COL = "│   ";
const TREE_TEE = "├── ";
const TREE_ELBOW = "└── ";
const TREE_SPACE = "    ";

function normalizeParentId<Id extends FlatTreeId>(
  parentId: Id | null | undefined,
): Id | null {
  return parentId ?? null;
}

function cloneItem<Id extends FlatTreeId, Content>(
  item: FlatTreeItem<Id, Content>,
): FlatTreeItem<Id, Content> {
  return {
    id: item.id,
    parentId: normalizeParentId(item.parentId),
    content: item.content,
  };
}

function stringifyNodeContent<Id extends FlatTreeId, Content>(
  node: FlatTreeNode<Id, Content>,
  stringify?: (content: Content, node: FlatTreeNode<Id, Content>) => string,
): string {
  if (stringify !== undefined) {
    return stringify(node.content, node);
  }

  if (typeof node.content === "string") {
    return node.content;
  }

  if (node.content !== null && typeof node.content === "object") {
    try {
      const text = JSON.stringify(node.content);
      if (text !== undefined) {
        return text;
      }
    } catch {
      // Fall through to String(value) for cyclic or otherwise unstringifiable objects.
    }
  }

  return String(node.content);
}

function renderNode<Id extends FlatTreeId, Content>(
  node: FlatTreeNode<Id, Content>,
  options: FlatTreeRenderOptions<Id, Content>,
  depth = 0,
  prefix = "",
  isLast = true,
  withBranch = false,
): string {
  const label = stringifyNodeContent(node, options.stringify);
  const branch = withBranch ? (isLast ? TREE_ELBOW : TREE_TEE) : "";
  let output = prefix + branch + label + "\n";
  const maxDepth = options.maxDepth;

  if (
    node.children.length === 0 ||
    (maxDepth !== undefined && depth >= maxDepth)
  ) {
    return output;
  }

  const nextPrefix =
    prefix + (withBranch ? (isLast ? TREE_SPACE : TREE_COL) : "");
  for (let index = 0; index < node.children.length; ++index) {
    output += renderNode(
      node.children[index],
      options,
      depth + 1,
      nextPrefix,
      index === node.children.length - 1,
      true,
    );
  }

  return output;
}

function removeChildById<Id extends FlatTreeId, Content>(
  children: MutableFlatTreeNode<Id, Content>[],
  childId: Id,
): void {
  const index = children.findIndex((child) => child.id === childId);
  if (index !== -1) {
    children.splice(index, 1);
  }
}

/**
 * A mutable tree view built from flat `(id, parentId, content)` items.
 *
 * Missing parents are treated as roots. Nodes may be added incrementally, and
 * parents may appear after children.
 */
export class FlatTree<Id extends FlatTreeId, Content> {
  private readonly itemList: FlatTreeItem<Id, Content>[] = [];
  private readonly itemById = new Map<Id, FlatTreeItem<Id, Content>>();
  private readonly nodeList: MutableFlatTreeNode<Id, Content>[] = [];
  private readonly nodeById = new Map<Id, MutableFlatTreeNode<Id, Content>>();
  private readonly pendingChildrenByParentId = new Map<
    Id,
    MutableFlatTreeNode<Id, Content>[]
  >();

  constructor(items: Iterable<FlatTreeItem<Id, Content>> = []) {
    this.addMany(items);
  }

  get size(): number {
    return this.itemList.length;
  }

  private requireNode(id: Id): MutableFlatTreeNode<Id, Content> {
    const node = this.nodeById.get(id);
    if (node === undefined) {
      throw new Error(`flat tree does not contain id: ${String(id)}`);
    }
    return node;
  }

  private isRootNode(node: MutableFlatTreeNode<Id, Content>): boolean {
    return node.parentId === null || !this.nodeById.has(node.parentId);
  }

  private addPendingChild(
    parentId: Id,
    child: MutableFlatTreeNode<Id, Content>,
  ): void {
    const pending = this.pendingChildrenByParentId.get(parentId) ?? [];
    if (!pending.some((candidate) => candidate.id === child.id)) {
      pending.push(child);
    }
    this.pendingChildrenByParentId.set(parentId, pending);
  }

  private removePendingChild(parentId: Id, childId: Id): void {
    const pending = this.pendingChildrenByParentId.get(parentId);
    if (pending === undefined) {
      return;
    }

    const next = pending.filter((child) => child.id !== childId);
    if (next.length === 0) {
      this.pendingChildrenByParentId.delete(parentId);
      return;
    }

    this.pendingChildrenByParentId.set(parentId, next);
  }

  private detachFromCurrentParent(
    node: MutableFlatTreeNode<Id, Content>,
  ): void {
    if (node.parentId === null) {
      return;
    }

    const parent = this.nodeById.get(node.parentId);
    if (parent !== undefined) {
      removeChildById(parent.children, node.id);
      return;
    }

    this.removePendingChild(node.parentId, node.id);
  }

  private attachToParentOrPending(
    node: MutableFlatTreeNode<Id, Content>,
  ): void {
    if (node.parentId === null) {
      return;
    }

    const parent = this.nodeById.get(node.parentId);
    if (parent !== undefined) {
      if (!parent.children.some((child) => child.id === node.id)) {
        parent.children.push(node);
      }
      return;
    }

    this.addPendingChild(node.parentId, node);
  }

  private attachPendingChildren(node: MutableFlatTreeNode<Id, Content>): void {
    const pending = this.pendingChildrenByParentId.get(node.id);
    if (pending === undefined) {
      return;
    }

    for (const child of pending) {
      if (
        child.parentId === node.id &&
        !node.children.some((candidate) => candidate.id === child.id)
      ) {
        node.children.push(child);
      }
    }

    this.pendingChildrenByParentId.delete(node.id);
  }

  private assertNoCycle(id: Id, parentId: Id | null): void {
    let currentParentId = parentId;
    while (currentParentId !== null) {
      if (currentParentId === id) {
        throw new Error(
          `flat tree contains a parent cycle: ${String(id)} -> ${String(parentId)}`,
        );
      }

      const parent = this.nodeById.get(currentParentId);
      if (parent === undefined) {
        return;
      }
      currentParentId = parent.parentId;
    }
  }

  has(id: Id): boolean {
    return this.nodeById.has(id);
  }

  node(id: Id): FlatTreeNode<Id, Content> | undefined {
    return this.nodeById.get(id);
  }

  nodes(): readonly FlatTreeNode<Id, Content>[] {
    return this.nodeList;
  }

  roots(): readonly FlatTreeNode<Id, Content>[] {
    return this.nodeList.filter((node) => this.isRootNode(node));
  }

  items(): FlatTreeItem<Id, Content>[] {
    return this.itemList.map((item) => cloneItem(item));
  }

  add(item: FlatTreeItem<Id, Content>): this {
    if (this.has(item.id)) {
      throw new Error(`flat tree contains duplicate id: ${String(item.id)}`);
    }
    return this.set(item);
  }

  addMany(items: Iterable<FlatTreeItem<Id, Content>>): this {
    for (const item of items) {
      this.add(item);
    }
    return this;
  }

  set(item: FlatTreeItem<Id, Content>): this {
    const nextItem = cloneItem(item);
    const nextParentId = normalizeParentId(nextItem.parentId);
    this.assertNoCycle(nextItem.id, nextParentId);

    const existingNode = this.nodeById.get(nextItem.id);
    if (existingNode === undefined) {
      const node: MutableFlatTreeNode<Id, Content> = {
        id: nextItem.id,
        parentId: nextParentId,
        content: nextItem.content,
        children: [],
      };

      this.itemList.push(nextItem);
      this.itemById.set(nextItem.id, nextItem);
      this.nodeList.push(node);
      this.nodeById.set(node.id, node);

      this.attachToParentOrPending(node);
      this.attachPendingChildren(node);
      return this;
    }

    const existingItem = this.itemById.get(nextItem.id);
    if (existingItem === undefined) {
      throw new Error(
        `flat tree item storage is corrupted for id: ${String(nextItem.id)}`,
      );
    }

    existingItem.parentId = nextParentId;
    existingItem.content = nextItem.content;

    const previousParentId = existingNode.parentId;
    existingNode.content = nextItem.content;

    if (previousParentId !== nextParentId) {
      this.detachFromCurrentParent(existingNode);
      existingNode.parentId = nextParentId;
      this.attachToParentOrPending(existingNode);
    }

    this.attachPendingChildren(existingNode);
    return this;
  }

  setMany(items: Iterable<FlatTreeItem<Id, Content>>): this {
    for (const item of items) {
      this.set(item);
    }
    return this;
  }

  parent(id: Id): FlatTreeNode<Id, Content> | undefined {
    const node = this.requireNode(id);
    if (node.parentId === null) {
      return undefined;
    }
    return this.nodeById.get(node.parentId);
  }

  father(id: Id): FlatTreeNode<Id, Content> | undefined {
    return this.parent(id);
  }

  children(id: Id): readonly FlatTreeNode<Id, Content>[] {
    return this.requireNode(id).children;
  }

  child(id: Id): readonly FlatTreeNode<Id, Content>[] {
    return this.children(id);
  }

  isAncestor(ancestorId: Id, descendantId: Id): boolean {
    this.requireNode(ancestorId);
    let current = this.requireNode(descendantId);

    while (current.parentId !== null) {
      if (current.parentId === ancestorId) {
        return true;
      }

      const parent = this.nodeById.get(current.parentId);
      if (parent === undefined) {
        return false;
      }
      current = parent;
    }

    return false;
  }

  isDescendant(descendantId: Id, ancestorId: Id): boolean {
    return this.isAncestor(ancestorId, descendantId);
  }

  relation(leftId: Id, rightId: Id): FlatTreeRelation {
    this.requireNode(leftId);
    this.requireNode(rightId);

    if (leftId === rightId) {
      return FlatTreeRelation.Self;
    }
    if (this.isAncestor(leftId, rightId)) {
      return FlatTreeRelation.Ancestor;
    }
    if (this.isAncestor(rightId, leftId)) {
      return FlatTreeRelation.Descendant;
    }
    return FlatTreeRelation.None;
  }

  render(options: FlatTreeRenderOptions<Id, Content> = {}): string {
    const roots = this.roots();
    if (roots.length === 0) {
      return "";
    }

    if (roots.length === 1) {
      return renderNode(roots[0], options);
    }

    let output = ".\n";
    for (let index = 0; index < roots.length; ++index) {
      output += renderNode(
        roots[index],
        options,
        0,
        "",
        index === roots.length - 1,
        true,
      );
    }
    return output;
  }

  toString(options: FlatTreeRenderOptions<Id, Content> = {}): string {
    return this.render(options);
  }

  print(options: FlatTreeRenderOptions<Id, Content> = {}): void {
    io.print(this.render(options));
  }
}
