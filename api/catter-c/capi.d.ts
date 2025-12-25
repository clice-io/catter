export {};
// io
export function stdout_print(content: string): void;
export function stdout_print_red(content: string): void;
export function stdout_print_yellow(content: string): void;
export function stdout_print_blue(content: string): void;
export function stdout_print_green(content: string): void;

// os
export function os_name(): "linux" | "windows" | "macos";
export function os_arch(): "x86" | "x64" | "arm" | "arm64";

// fs
export function fs_exists(path: string): boolean;
export function fs_is_file(path: string): boolean;
export function fs_is_dir(path: string): boolean;
export function fs_pwd(): string;
export function fs_path_join_all(paths: string[]): string;
export function fs_path_ancestor_n(path: string, n: number): string;
export function fs_path_filename(path: string): string;
export function fs_path_extension(path: string): string;
export function fs_path_absolute(path: string): string;
export function fs_path_lexical_normal(path: string): string;
export function fs_path_relative_to(base: string, path: string): string;

export function fs_create_dir_recursively(path: string): boolean;
export function fs_create_empty_file_recursively(path: string): boolean;

export function fs_remove_recursively(path: string): void;
export function fs_rename_if_exists(
  old_path: string,
  new_path: string,
): boolean;

export function fs_list_dir(path: string): string[];

// io read/write raw binary stream
export function file_open(path: string): number;
export function file_close(fd: number): void;
export function file_seek_write(
  fd: number,
  offset: number,
  whence: number,
): void;
export function file_seek_read(
  fd: number,
  offset: number,
  whence: number,
): void;
export function file_tell_write(fd: number): number;
export function file_tell_read(fd: number): number;
export function file_read_n(
  fd: number,
  buf_size: number,
  buf: ArrayBuffer,
): number;
export function file_write_n(
  fd: number,
  buf_size: number,
  buf: ArrayBuffer,
): void;
