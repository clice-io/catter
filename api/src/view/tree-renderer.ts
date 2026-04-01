/**
 * Identifier type accepted by {@link TreeRenderer}.
 */
export type TreeId = PropertyKey;

/**
 * Input contract for {@link TreeRenderer}.
 */
export interface TreeRenderInput<Id extends TreeId, Content> {
  /**
   * First id used by traversal.
   *
   * Use `undefined` when the virtual root has multiple top-level children.
   */
  first: Id | undefined;
  /**
   * Returns child ids for one node.
   */
  children(id: Id | undefined): readonly Id[];
  /**
   * Returns content for one node id.
   */
  content(id: Id): Content | undefined;
}

/**
 * CLI output options.
 */
export interface TreeOutputCliOptions<Id extends TreeId, Content> {
  /** Output format type. */
  type: "cli";
  /** Optional depth limit. `0` prints only roots. */
  maxDepth?: number;
  /** Optional text formatter. */
  text?: (content: Content, id: Id) => string;
}

/**
 * Supported renderer output options.
 */
export type TreeOutputOptions<
  Id extends TreeId,
  Content,
> = TreeOutputCliOptions<Id, Content>;

const TREE_COL = "│   ";
const TREE_TEE = "├── ";
const TREE_ELBOW = "└── ";
const TREE_SPACE = "    ";
const ANSI_RESET = "\u001b[0m";
const DEPTH_COLOR_CODES = [
  "\u001b[34m",
  "\u001b[32m",
  "\u001b[33m",
  "\u001b[31m",
] as const;

/**
 * Generic tree renderer.
 */
export class TreeRenderer<Id extends TreeId, Content> {
  private readonly firstId: Id | undefined;
  private readonly childrenOf: (id: Id | undefined) => readonly Id[];
  private readonly contentOf: (id: Id) => Content | undefined;

  /**
   * Builds a renderer from tree input callbacks.
   */
  constructor(input: TreeRenderInput<Id, Content>) {
    this.firstId = input.first;
    this.childrenOf = input.children;
    this.contentOf = input.content;
  }

  /**
   * Renders output in one of the supported formats.
   */
  output(options: TreeOutputOptions<Id, Content>): string {
    switch (options.type) {
      case "cli":
        return this.outputCli(options);
    }
  }

  private outputCli(options: TreeOutputCliOptions<Id, Content>): string {
    const roots =
      this.firstId === undefined
        ? [...this.childrenOf(undefined)]
        : [this.firstId];

    if (roots.length === 0) {
      return "";
    }

    const textOf =
      options.text ??
      ((content: Content) => {
        if (typeof content === "string") {
          return content;
        }

        if (content !== null && typeof content === "object") {
          try {
            const text = JSON.stringify(content);
            if (text !== undefined) {
              return text;
            }
          } catch {
            // Fall through to String(value).
          }
        }

        return String(content);
      });

    const walk = (
      id: Id,
      depth: number,
      prefix: string,
      isLast: boolean,
      withBranch: boolean,
      path: Set<Id>,
    ): string => {
      if (path.has(id)) {
        return "";
      }

      const content = this.contentOf(id);
      if (content === undefined) {
        return "";
      }

      path.add(id);
      try {
        const color = DEPTH_COLOR_CODES[depth % DEPTH_COLOR_CODES.length];
        const branch = withBranch ? (isLast ? TREE_ELBOW : TREE_TEE) : "";
        let out = `${color}${prefix}${branch}${textOf(content, id)}${ANSI_RESET}\n`;

        if (options.maxDepth !== undefined && depth >= options.maxDepth) {
          return out;
        }

        const children: Id[] = [];
        for (const childId of this.childrenOf(id)) {
          if (
            childId === id ||
            path.has(childId) ||
            children.includes(childId)
          ) {
            continue;
          }
          children.push(childId);
        }

        if (children.length === 0) {
          return out;
        }

        const nextPrefix =
          prefix + (withBranch ? (isLast ? TREE_SPACE : TREE_COL) : "");
        for (let index = 0; index < children.length; ++index) {
          out += walk(
            children[index],
            depth + 1,
            nextPrefix,
            index === children.length - 1,
            true,
            path,
          );
        }
        return out;
      } finally {
        path.delete(id);
      }
    };

    if (roots.length === 1) {
      return walk(roots[0], 0, "", true, false, new Set<Id>());
    }

    let out = ".\n";
    for (let index = 0; index < roots.length; ++index) {
      out += walk(
        roots[index],
        0,
        "",
        index === roots.length - 1,
        true,
        new Set<Id>(),
      );
    }
    return out;
  }
}
