import * as fs from "../fs.js";
import * as io from "../io.js";

/**
 * A single compile_commands.json entry.
 *
 * The Clang compilation database format allows either `command` or
 * `arguments` to describe the compiler invocation. This type accepts both
 * variants and preserves extra fields when loading an existing database.
 */
export type CDBItem = {
  directory: string;
  file: string;
  command?: string;
  arguments?: string[];
  output?: string;
  [key: string]: unknown;
};

export type CDBManagerOptions = {
  /**
   * Whether to load and merge entries from the existing database file.
   *
   * Defaults to `true` for direct manager usage. Script frontends can set this
   * to `false` when they want a fresh output file.
   */
  inherit?: boolean;
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isStringList(value: unknown): value is string[] {
  return (
    Array.isArray(value) && value.every((part) => typeof part === "string")
  );
}

function cloneItem(item: CDBItem): CDBItem {
  return {
    ...item,
    arguments: item.arguments === undefined ? undefined : [...item.arguments],
  };
}

function readEntireText(path: string): string {
  let content = "";
  io.TextFileStream.with(path, "utf-8", (stream) => {
    content = stream.readEntireFile();
  });
  return content;
}

function asItem(value: unknown, context: string): CDBItem {
  if (!isRecord(value)) {
    throw new Error(`${context}: expected object item`);
  }

  const directory = value.directory;
  if (typeof directory !== "string" || directory.length === 0) {
    throw new Error(`${context}: "directory" must be a non-empty string`);
  }

  const file = value.file;
  if (typeof file !== "string" || file.length === 0) {
    throw new Error(`${context}: "file" must be a non-empty string`);
  }

  const command = value.command;
  if (command !== undefined && typeof command !== "string") {
    throw new Error(`${context}: "command" must be a string`);
  }

  const argumentsValue = value.arguments;
  if (argumentsValue !== undefined && !isStringList(argumentsValue)) {
    throw new Error(`${context}: "arguments" must be a string array`);
  }

  if (command === undefined && argumentsValue === undefined) {
    throw new Error(`${context}: expected "command" or "arguments"`);
  }

  const output = value.output;
  if (output !== undefined && typeof output !== "string") {
    throw new Error(`${context}: "output" must be a string`);
  }

  const item: CDBItem = {
    ...value,
    directory,
    file,
  };
  if (command !== undefined) {
    item.command = command;
  }
  if (argumentsValue !== undefined) {
    item.arguments = [...argumentsValue];
  }
  if (output !== undefined) {
    item.output = output;
  }
  return item;
}

function fileKey(item: CDBItem): string {
  const base = fs.path.absolute(item.directory);
  const file = fs.path.isAbsolute(item.file)
    ? item.file
    : fs.path.joinAll(base, item.file);
  return fs.path.lexicalNormal(file);
}

function itemKey(item: CDBItem): string {
  return JSON.stringify([
    item.output ?? "",
    item.command ?? "",
    item.arguments ?? [],
  ]);
}

function addTo(store: Map<string, Map<string, CDBItem>>, item: CDBItem): void {
  const file = fileKey(item);
  let items = store.get(file);
  if (items === undefined) {
    items = new Map();
    store.set(file, items);
  }
  items.set(itemKey(item), cloneItem(item));
}

function readItemsFromPath(path: string): CDBItem[] {
  if (!fs.exists(path)) {
    return [];
  }
  if (!fs.isFile(path)) {
    throw new Error(`CDB path is not a file: ${path}`);
  }

  const raw = readEntireText(path).trim();
  if (raw.length === 0) {
    return [];
  }

  const parsed: unknown = JSON.parse(raw);
  if (!Array.isArray(parsed)) {
    throw new Error(`CDB file must contain a JSON array: ${path}`);
  }

  return parsed.map((item, index) => asItem(item, `${path}[${index}]`));
}

function writeItemsToPath(path: string, items: CDBItem[]): void {
  if (fs.exists(path)) {
    fs.removeAll(path);
  }
  fs.createFile(path, true);

  io.TextFileStream.with(path, "utf-8", (stream) => {
    stream.write(JSON.stringify(items, null, 2));
  });
}

/**
 * A manager for Clang JSON compilation databases.
 *
 * When constructed with an existing `compile_commands.json` path, previous
 * entries are inherited. New items override existing entries that resolve to
 * the same source file; untouched inherited entries are preserved on save.
 */
export class CDBManager {
  readonly savePath: string;

  private readonly inheritedItems = new Map<string, Map<string, CDBItem>>();
  private readonly pendingItems = new Map<string, Map<string, CDBItem>>();

  constructor(
    savePath: string = "compile_commands.json",
    options: CDBManagerOptions = {},
  ) {
    this.savePath = savePath;

    if (options.inherit ?? true) {
      for (const item of readItemsFromPath(savePath)) {
        addTo(this.inheritedItems, item);
      }
    }
  }

  private mergedItems(): CDBItem[] {
    const items: CDBItem[] = [];

    for (const [file, group] of this.inheritedItems) {
      if (!this.pendingItems.has(file)) {
        for (const item of group.values()) {
          items.push(cloneItem(item));
        }
      }
    }

    for (const group of this.pendingItems.values()) {
      for (const item of group.values()) {
        items.push(cloneItem(item));
      }
    }

    return items;
  }

  /**
   * Adds or replaces a single compilation database item.
   *
   * Items are grouped by the normalized absolute source-file path derived from
   * `directory` and `file`. Adding any new item for a source file replaces all
   * inherited items for that source file when saved.
   */
  addItem(item: CDBItem): this {
    addTo(this.pendingItems, asItem(item, "CDBManager.addItem"));
    return this;
  }

  /**
   * Merges entries from another manager or an iterable list of items.
   *
   * Later items override earlier ones when they resolve to the same source
   * file.
   */
  merge(other: CDBManager | Iterable<CDBItem>): this {
    const items = other instanceof CDBManager ? other.items() : other;
    for (const item of items) {
      this.addItem(item);
    }
    return this;
  }

  /**
   * Returns the current merged view of the database.
   */
  items(): CDBItem[] {
    return this.mergedItems();
  }

  /**
   * Returns the merged view in a JSON-serializable form.
   */
  toJSON(): CDBItem[] {
    return this.items();
  }

  /**
   * Saves the merged compilation database to disk.
   *
   * If `path` is omitted, the constructor path is used.
   */
  save(path?: string): string {
    const targetPath = path ?? this.savePath;
    writeItemsToPath(targetPath, this.mergedItems());
    return targetPath;
  }
}
