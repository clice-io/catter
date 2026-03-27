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

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function cloneItem(item: CDBItem): CDBItem {
  return {
    ...item,
    arguments: item.arguments === undefined ? undefined : [...item.arguments],
  };
}

function readEntireText(path: string): string {
  let content = "";
  io.TextFileStream.with(path, "ascii", (stream) => {
    content = stream.readEntireFile();
  });
  return content;
}

function validateItem(value: unknown, context: string): CDBItem {
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
  if (argumentsValue !== undefined) {
    if (
      !Array.isArray(argumentsValue) ||
      !argumentsValue.every((arg) => typeof arg === "string")
    ) {
      throw new Error(`${context}: "arguments" must be a string array`);
    }
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

function itemKey(item: CDBItem): string {
  return fs.path.lexicalNormal(
    fs.path.absolute(fs.path.joinAll(item.directory, item.file)),
  );
}

function setItem(store: Map<string, CDBItem>, item: CDBItem): void {
  const key = itemKey(item);
  store.delete(key);
  store.set(key, cloneItem(item));
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

  return parsed.map((item, index) => validateItem(item, `${path}[${index}]`));
}

function writeItemsToPath(path: string, items: CDBItem[]): void {
  if (fs.exists(path)) {
    fs.removeAll(path);
  }
  fs.createFile(path, true);

  io.TextFileStream.with(path, "ascii", (stream) => {
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

  private readonly inheritedItems = new Map<string, CDBItem>();
  private readonly pendingItems = new Map<string, CDBItem>();

  constructor(savePath: string = "compile_commands.json") {
    this.savePath = savePath;

    for (const item of readItemsFromPath(savePath)) {
      setItem(this.inheritedItems, item);
    }
  }

  private mergedItems(): CDBItem[] {
    const items: CDBItem[] = [];

    for (const [key, item] of this.inheritedItems) {
      if (!this.pendingItems.has(key)) {
        items.push(cloneItem(item));
      }
    }

    for (const item of this.pendingItems.values()) {
      items.push(cloneItem(item));
    }

    return items;
  }

  /**
   * Adds or replaces a single compilation database item.
   *
   * Items are keyed by the normalized absolute source-file path derived from
   * `directory` and `file`.
   */
  addItem(item: CDBItem): this {
    setItem(this.pendingItems, validateItem(item, "CDBManager.addItem"));
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
