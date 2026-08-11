import {
  fs_create_dir_recursively,
  fs_create_empty_file_recursively,
  fs_async_create_dir_recursively,
  fs_async_create_empty_file_recursively,
  fs_async_exists,
  fs_async_is_dir,
  fs_async_is_file,
  fs_async_list_dir,
  fs_async_read_text,
  fs_async_remove_recursively,
  fs_async_rename_if_exists,
  fs_async_write_text,
  fs_exists,
  fs_is_dir,
  fs_is_file,
  fs_list_dir,
  fs_path_absolute,
  fs_path_ancestor_n,
  fs_path_extension,
  fs_path_filename,
  fs_path_join_all,
  fs_path_lexical_normal,
  fs_path_relative_to,
  fs_pwd,
  fs_remove_recursively,
  fs_rename_if_exists,
} from "catter/native";

export {};

/**
 * Checks whether a path exists in the filesystem.
 *
 * @param pathStr - The filesystem path to check. Can be relative or absolute.
 * @returns `true` if the path exists, `false` otherwise.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error
 *         or system exception.
 *
 * @example
 * ```typescript
 * if (existsSync('/home/user/file.txt')) {
 *   console.log('File exists');
 * }
 * ```
 */
export function existsSync(pathStr: string): boolean {
  return fs_exists(pathStr);
}

/**
 * Checks whether a path points to a regular file.
 *
 * @param pathStr - The filesystem path to check. Can be relative or absolute.
 * @returns `true` if the path is a regular file, `false` otherwise.
 *          Returns `false` if the path is a directory or does not exist.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error.
 *
 * @example
 * ```typescript
 * if (isFileSync('./config.json')) {
 *   console.log('Config is a file');
 * }
 * ```
 */
export function isFileSync(pathStr: string): boolean {
  return fs_is_file(pathStr);
}

/**
 * Checks whether a path points to a directory.
 *
 * @param pathStr - The filesystem path to check. Can be relative or absolute.
 * @returns `true` if the path is a directory, `false` otherwise.
 *          Returns `false` if the path is a file or does not exist.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error.
 *
 * @example
 * ```typescript
 * if (isDirSync('./src')) {
 *   console.log('src is a directory');
 * }
 * ```
 */
export function isDirSync(pathStr: string): boolean {
  return fs_is_dir(pathStr);
}

/**
 * Gets the current working directory from the global runtime configuration.
 *
 * @returns The absolute path to the current working directory as a string.
 *
 * @example
 * ```typescript
 * const cwd = pwd();
 * println("Current directory: " + cwd);
 * ```
 */
export function pwd(): string {
  return fs_pwd();
}

/**
 * Lists all entries (files and directories) in a directory.
 *
 * @param pathStr - The path to the directory to read. Can be relative or absolute.
 * @returns An array of absolute paths to all entries within the directory.
 * @throws Will throw an error if `pathStr` is not a directory or if the underlying
 *         C layer encounters a filesystem error.
 *
 * @example
 * ```typescript
 * const entries = readDirsSync('./src');
 * for (let i = 0; i < entries.length; i++) {
 *   println(entries[i]);
 * }
 * ```
 */
export function readDirsSync(pathStr: string): string[] {
  return fs_list_dir(pathStr);
}

/**
 * Creates a directory at the specified path.
 *
 * When `recursively` is `true`, creates all missing parent directories as needed
 * (equivalent to `mkdir -p`). When `false`, only creates the directory if its
 * parent exists.
 *
 * @param pathStr - The path of the directory to create. Can be relative or absolute.
 * @param recursively - Whether to create parent directories as needed. Defaults to `true`.
 * @returns `true` if the directory was created or parent directories were ensured successfully.
 *          `false` if `recursively` is `false` and the parent directory does not exist.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error.
 *
 * @example
 * ```typescript
 * // Create nested directories
 * mkdirSync('./path/to/deep/dir');
 *
 * // Create only if parent exists
 * mkdirSync('./existing/subdir', false);
 * ```
 */
export function mkdirSync(pathStr: string, recursively = true): boolean {
  if (recursively) {
    fs_create_dir_recursively(pathStr);
    return true;
  }
  if (isDirSync(path.toAncestor(pathStr))) {
    fs_create_dir_recursively(pathStr);
    return true;
  }
  return false;
}

/**
 * Creates an empty file at the specified path.
 *
 * When `recursively` is `true`, ensures all parent directories exist before creating
 * the file. When `false`, only creates the file if its parent directory exists.
 * The file is created using append mode; if it already exists, it is left unchanged.
 *
 * @param pathStr - The path of the file to create. Can be relative or absolute.
 * @param recursively - Whether to ensure parent directories exist. Defaults to `true`.
 * @returns `true` if parent directories were ensured and the file creation succeeded.
 *          `false` if `recursively` is `false` and the parent directory does not exist.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error
 *         while creating parent directories.
 *
 * @example
 * ```typescript
 * // Create file with parent directories
 * createFileSync('./logs/app/error.log');
 *
 * // Create only if parent exists
 * createFileSync('./existing/file.txt', false);
 * ```
 */
export function createFileSync(pathStr: string, recursively = true): boolean {
  if (recursively) {
    fs_create_empty_file_recursively(pathStr);
    return true;
  }
  if (isDirSync(path.toAncestor(pathStr))) {
    fs_create_empty_file_recursively(pathStr);
    return true;
  }
  return false;
}

/**
 * Recursively removes a file or directory and all its contents.
 *
 * If the path is a directory, removes the directory and all files/subdirectories within it.
 * If the path is a file, removes only that file.
 *
 * @param pathStr - The path to remove. Can be relative or absolute.
 * @throws Will throw an error if the underlying C layer encounters a filesystem error.
 *
 * @example
 * ```typescript
 * // Remove a file
 * removeAllSync('./temp.txt');
 *
 * // Remove a directory and all its contents
 * removeAllSync('./build');
 * ```
 */
export function removeAllSync(pathStr: string): void {
  return fs_remove_recursively(pathStr);
}

/**
 * Renames or moves a file or directory.
 *
 * Moves the item from `oldPath` to `newPath`. Both paths are converted to absolute
 * paths before the operation. If the old path does not exist, the operation fails
 * gracefully without throwing.
 *
 * @param oldPath - The current path of the file or directory. Can be relative or absolute.
 * @param newPath - The target path. Can be relative or absolute.
 * @returns `true` if the rename/move succeeded, `false` if `oldPath` does not exist.
 * @throws Will throw an error if the rename operation fails (e.g., permission denied,
 *         target path already exists with different semantics).
 *
 * @example
 * ```typescript
 * // Rename a file
 * renameSync('./old-name.txt', './new-name.txt');
 *
 * // Move to different directory
 * renameSync('./src/file.ts', './dist/file.ts');
 * ```
 */
export function renameSync(oldPath: string, newPath: string): boolean {
  return fs_rename_if_exists(oldPath, newPath);
}

/**
 * Asynchronously checks whether a path exists.
 */
export function exists(pathStr: string): Promise<boolean> {
  return fs_async_exists(pathStr);
}

/**
 * Asynchronously checks whether a path points to a regular file.
 */
export function isFile(pathStr: string): Promise<boolean> {
  return fs_async_is_file(pathStr);
}

/**
 * Asynchronously checks whether a path points to a directory.
 */
export function isDir(pathStr: string): Promise<boolean> {
  return fs_async_is_dir(pathStr);
}

/**
 * Asynchronously lists all entries in a directory.
 */
export function readDirs(pathStr: string): Promise<string[]> {
  return fs_async_list_dir(pathStr);
}

/**
 * Asynchronously creates a directory.
 */
export async function mkdir(
  pathStr: string,
  recursively = true,
): Promise<boolean> {
  if (recursively) {
    await fs_async_create_dir_recursively(pathStr);
    return true;
  }
  if (await isDir(path.toAncestor(pathStr))) {
    await fs_async_create_dir_recursively(pathStr);
    return true;
  }
  return false;
}

/**
 * Asynchronously creates an empty file.
 */
export async function createFile(
  pathStr: string,
  recursively = true,
): Promise<boolean> {
  if (recursively) {
    await fs_async_create_empty_file_recursively(pathStr);
    return true;
  }
  if (await isDir(path.toAncestor(pathStr))) {
    await fs_async_create_empty_file_recursively(pathStr);
    return true;
  }
  return false;
}

/**
 * Asynchronously removes a file or directory tree.
 */
export async function removeAll(pathStr: string): Promise<void> {
  return fs_async_remove_recursively(pathStr);
}

/**
 * Asynchronously renames or moves a file or directory.
 */
export async function rename(
  oldPath: string,
  newPath: string,
): Promise<boolean> {
  return fs_async_rename_if_exists(oldPath, newPath);
}

/**
 * Asynchronously reads a text file as a string.
 */
export async function readText(pathStr: string): Promise<string> {
  return fs_async_read_text(pathStr);
}

/**
 * Asynchronously writes a string to a text file.
 */
export async function writeText(
  pathStr: string,
  content: string,
): Promise<void> {
  return fs_async_write_text(pathStr, content);
}

/**
 * Utilities for filesystem path manipulation.
 *
 * All path operations work with both relative and absolute paths.
 * Relative paths are resolved against the current working directory.
 *
 * @example
 * ```typescript
 * const cacheFile = path.joinAll(pwd(), ".cache", "result.json");
 * ```
 */
export namespace path {
  /**
   * Checks whether a path string is absolute.
   *
   * Supports POSIX roots (`/foo`), Windows drive roots (`C:\\foo`), and UNC
   * paths (`\\\\server\\share`).
   *
   * @param pathStr - The path to inspect.
   * @returns `true` if the path is absolute.
   *
   * @example
   * ```typescript
   * if (path.isAbsolute('/tmp/file.txt')) {
   *   println('absolute');
   * }
   * ```
   */
  export function isAbsolute(pathStr: string): boolean {
    return (
      pathStr.startsWith("/") ||
      pathStr.startsWith("\\\\") ||
      /^[A-Za-z]:[\\/]/.test(pathStr)
    );
  }
  /**
   * Joins multiple path segments into a single path string.
   *
   * Handles path separators correctly across platforms. The first segment
   * can be absolute; subsequent segments are appended with proper separators.
   *
   * @param paths - Variable number of path segments to join.
   * @returns The concatenated path string using the platform-specific separator.
   *
   * @example
   * ```typescript
   * const fullPath = path.joinAll('/home', 'user', 'documents', 'file.txt');
   * // Returns: '/home/user/documents/file.txt'
   * ```
   */
  export function joinAll(...paths: string[]): string {
    return fs_path_join_all(paths);
  }

  /**
   * Converts a path to its absolute form.
   *
   * If the path is already absolute, it is returned as-is. If the path is relative,
   * it is resolved against the current working directory.
   *
   * @param path - The path to resolve. Can be relative or absolute.
   * @returns The absolute path string.
   *
   * @example
   * ```typescript
   * const abs = path.absolute('./relative/path');
   * // Returns: '/home/user/current-dir/relative/path'
   * ```
   */
  export function absolute(path: string): string {
    return fs_path_absolute(path);
  }

  /**
   * Gets the ancestor directory (parent, grandparent, etc.) of a path.
   *
   * Ascends `n` levels up the directory hierarchy. If the path does not have
   * that many ancestors, returns the root directory.
   *
   * @param path - The starting path. Can be relative or absolute.
   * @param n - The number of levels to ascend. Defaults to 1 (direct parent).
   * @returns The ancestor path string. If `n` exceeds the depth, returns root.
   *
   * @example
   * ```typescript
   * const parent = path.toAncestor('/home/user/docs/file.txt');
   * // Returns: '/home/user/docs'
   *
   * const grandparent = path.toAncestor('/home/user/docs/file.txt', 2);
   * // Returns: '/home/user'
   * ```
   */
  export function toAncestor(path: string, n: number = 1) {
    return fs_path_ancestor_n(path, n);
  }

  /**
   * Gets the file extension of a path, including the leading dot.
   *
   * Returns only the extension part (e.g., ".txt", ".json"). If the path has
   * no extension, returns an empty string.
   *
   * @param path - The file path. Can be relative or absolute.
   * @returns The extension string including the dot, or empty string if no extension.
   *
   * @example
   * ```typescript
   * const ext1 = path.extension('/path/to/file.txt');
   * // Returns: '.txt'
   *
   * const ext2 = path.extension('/path/to/README');
   * // Returns: ''
   * ```
   */
  export function extension(path: string): string {
    return fs_path_extension(path);
  }

  /**
   * Computes the relative path from a base directory to a target path.
   *
   * Both paths are converted to absolute form before computing the relative path.
   * The result is a path that, when joined with the base, yields the target path.
   *
   * @param base - The base directory path. Can be relative or absolute.
   * @param path - The target path. Can be relative or absolute.
   * @returns The relative path from base to target.
   *
   * @example
   * ```typescript
   * const rel = path.relativeTo('/home/user', '/home/user/docs/file.txt');
   * // Returns: 'docs/file.txt'
   * ```
   */
  export function relativeTo(base: string, path: string): string {
    return fs_path_relative_to(base, path);
  }

  /**
   * Gets the filename component (the last segment) of a path.
   *
   * Returns only the filename, not the directory path. For a directory path,
   * returns the directory name itself.
   *
   * @param path - The file or directory path. Can be relative or absolute.
   * @returns The filename/directory name string.
   *
   * @example
   * ```typescript
   * const name1 = path.filename('/home/user/file.txt');
   * // Returns: 'file.txt'
   *
   * const name2 = path.filename('/home/user/');
   * // Returns: 'user'
   * ```
   */
  export function filename(path: string): string {
    return fs_path_filename(path);
  }

  /**
   * Converts a path to its lexically normalized form.
   * Removes redundant components like "." and ".." without accessing the filesystem and replace the separators to platform-specific ones.
   *
   * See https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal.
   *
   * @param path - The input path string.
   * @returns The lexically normalized path string.
   *
   * @example
   * ```typescript
   * const normalized = path.lexicalNormal("src/./generated/../main.cc");
   * // "src/main.cc"
   * ```
   */
  export function lexicalNormal(path: string): string {
    return fs_path_lexical_normal(path);
  }
}
