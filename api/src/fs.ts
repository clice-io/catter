import {
  fs_create_dir_recursively,
  fs_create_empty_file_recursively,
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
} from "catter-c";

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
 * if (exists('/home/user/file.txt')) {
 *   console.log('File exists');
 * }
 * ```
 */
export function exists(pathStr: string): boolean {
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
 * if (isFile('./config.json')) {
 *   console.log('Config is a file');
 * }
 * ```
 */
export function isFile(pathStr: string): boolean {
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
 * if (isDir('./src')) {
 *   console.log('src is a directory');
 * }
 * ```
 */
export function isDir(pathStr: string): boolean {
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
 * const entries = readDirs('./src');
 * for (let i = 0; i < entries.length; i++) {
 *   println(entries[i]);
 * }
 * ```
 */
export function readDirs(pathStr: string): string[] {
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
 * mkdir('./path/to/deep/dir');
 *
 * // Create only if parent exists
 * mkdir('./existing/subdir', false);
 * ```
 */
export function mkdir(pathStr: string, recursively = true): boolean {
  if (recursively) {
    fs_create_dir_recursively(pathStr);
    return true;
  }
  if (isDir(path.toAncestor(pathStr))) {
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
 * createFile('./logs/app/error.log');
 *
 * // Create only if parent exists
 * createFile('./existing/file.txt', false);
 * ```
 */
export function createFile(pathStr: string, recursively = true): boolean {
  if (recursively) {
    fs_create_empty_file_recursively(pathStr);
    return true;
  }
  if (isDir(path.toAncestor(pathStr))) {
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
 * removeAll('./temp.txt');
 *
 * // Remove a directory and all its contents
 * removeAll('./build');
 * ```
 */
export function removeAll(pathStr: string): void {
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
 * rename('./old-name.txt', './new-name.txt');
 *
 * // Move to different directory
 * rename('./src/file.ts', './dist/file.ts');
 * ```
 */
export function rename(oldPath: string, newPath: string): boolean {
  return fs_rename_if_exists(oldPath, newPath);
}

/**
 * Utilities for filesystem path manipulation.
 *
 * All path operations work with both relative and absolute paths.
 * Relative paths are resolved against the current working directory.
 */
export const path = {
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
  joinAll(...paths: string[]): string {
    return fs_path_join_all(paths);
  },

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
  absolute(path: string): string {
    return fs_path_absolute(path);
  },

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
  toAncestor(path: string, n: number = 1) {
    return fs_path_ancestor_n(path, n);
  },

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
  extension(path: string): string {
    return fs_path_extension(path);
  },

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
  relativeTo(base: string, path: string): string {
    return fs_path_relative_to(base, path);
  },

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
  filename(path: string): string {
    return fs_path_filename(path);
  },

  /**
   * Converts a path to its lexically normalized form.
   * Removes redundant components like "." and ".." without accessing the filesystem and replace the separators to platform-specific ones.
   *
   * See https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal.
   *
   * @param path - The input path string.
   * @returns The lexically normalized path string.
   */
  lexicalNormal(path: string): string {
    return fs_path_lexical_normal(path);
  },
};
