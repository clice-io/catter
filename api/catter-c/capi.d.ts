export {};

/**
 * Result returned from a command handler.
 */
export type Action =
  | {
      /**
       * Ignore the command in catter, but still execute the original command.
       */
      type: "skip";
    }
  | {
      /**
       * Skip execution of the original command.
       */
      type: "drop";
    }
  | {
      /**
       * Abort the whole execution and report an error.
       */
      type: "abort";
    }
  | {
      /**
       * Replace the original command with a modified command.
       */
      type: "modify";

      /**
       * Replacement command data for the modified command.
       */
      data: CommandData;
    };

/**
 * Action discriminator extracted from {@link Action}.
 */
export type ActionType = Action["type"];

/**
 * Event emitted while a command is executing.
 */
export type ExecutionEvent =
  | {
      /**
       * Event category.
       */
      type: "output";

      /**
       * Standard output content for an `"output"` event.
       */
      stdout: string;

      /**
       * Standard error content for an `"output"` event.
       */
      stderr: string;

      /**
       * Runtime-defined status code for this output event.
       */
      code: number;
    }
  | {
      /**
       * Event category.
       */
      type: "finish";

      /**
       * Final process exit code.
       */
      code: number;
    };

/**
 * Execution event discriminator extracted from {@link ExecutionEvent}.
 */
export type EventType = ExecutionEvent["type"];

/**
 * Runtime capabilities exposed to the script.
 */
export type CatterRuntime = {
  /**
   * Actions supported by the current runtime.
   */
  supportActions: ActionType[];

  /**
   * Execution event kinds supported by the current runtime.
   */
  supportEvents: EventType[];

  /**
   * Runtime implementation type.
   *
   * - `"inject"`: Native injection-based runtime.
   * - `"eslogger"`: macOS event-stream logger runtime.
   * - `"env"`: Environment-based runtime, for example when `CC=catter-proxy`.
   */
  type: "inject" | "eslogger" | "env";

  /**
   * Whether captured commands can report a parent command identifier.
   */
  supportParentId: boolean;
};

/**
 * Configuration passed to the script before command capture begins.
 */
export type CatterConfig = {
  /**
   * Absolute path to the current script.
   */
  scriptPath: string;

  /**
   * Command-line arguments passed to the script.
   */
  scriptArgs: string[];

  /**
   * Build-system command used to launch the script, for example `['bazel', 'build', '//:target']`.
   */
  buildSystemCommand: string[];

  /**
   * Runtime capabilities available for the current script execution.
   */
  runtime: CatterRuntime;

  /**
   * Optional catter runtime features.
   */
  options: {
    /**
     * Enables runtime logging.
     */
    log: boolean;
  };

  /**
   * Whether catter should execute the build-system command after `onStart`.
   *
   * When `false`, catter stops before launching the build.
   */
  execute: boolean;
};

/**
 * Error payload returned when command capture fails.
 */
export type CatterErr = {
  /**
   * Error message describing the failure reason.
   */
  msg: string;
};

/**
 * Captured command metadata.
 */
export type CommandData = {
  /**
   * Working directory of the command.
   */
  cwd: string;

  /**
   * Executable path or name.
   */
  exe: string;

  /**
   * Full argument vector, including the executable arguments.
   */
  argv: string[];

  /**
   * Environment variables in `KEY=VALUE` form.
   */
  env: string[];

  /**
   * Runtime that captured this command.
   */
  runtime: CatterRuntime;

  /**
   * Parent command identifier when the runtime supports parent tracking.
   */
  parent?: number;
};

/**
 * Tagged command capture result passed to `service_on_command`.
 */
export type CommandCaptureResult =
  | {
      /**
       * Indicates command capture succeeded.
       */
      success: true;

      /**
       * Captured command payload.
       */
      data: CommandData;
    }
  | {
      /**
       * Indicates command capture failed.
       */
      success: false;

      /**
       * Failure details for command capture.
       */
      error: CatterErr;
    };

export function service_on_start(
  cb: (config: CatterConfig) => CatterConfig,
): void;
export function service_on_finish(cb: (event: ExecutionEvent) => void): void;
export function service_on_command(
  cb: (id: number, data: CommandCaptureResult) => Action,
): void;
export function service_on_execution(
  cb: (id: number, event: ExecutionEvent) => void,
): void;
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

// option
export type OptionItem = {
  values: string[];
  key: string;
  id: number;
  unalias?: number;
  index: number;
};

export type OptionTable =
  | "clang"
  | "lld-coff"
  | "lld-elf"
  | "lld-macho"
  | "lld-mingw"
  | "lld-wasm"
  | "nvcc"
  | "llvm-dlltool"
  | "llvm-lib";

export enum OptionKindClass {
  GroupClass = 0,
  InputClass,
  UnknownClass,
  FlagClass,
  JoinedClass,
  ValuesClass,
  SeparateClass,
  RemainingArgsClass,
  RemainingArgsJoinedClass,
  CommaJoinedClass,
  MultiArgClass,
  JoinedOrSeparateClass,
  JoinedAndSeparateClass,
}

export type OptionInfo = {
  id: number;
  prefixedKey: string;
  kind: OptionKindClass;
  group: number;
  alias: number;
  aliasArgs: string[];
  flags: number;
  visibility: number;
  param: number;
  help: string;
  meta_var: string;
};

export function option_get_info(table: OptionTable, id: number): OptionInfo;

/**
 *
 * @param args from argv[1]
 */
export function option_parse(
  table: OptionTable,
  args: string[],
  cb: (parseRes: string | OptionItem) => boolean,
  visibility?: number,
): void;

export type Compiler =
  | "gcc"
  | "clang"
  | "flang"
  | "ifort"
  | "crayftn"
  | "nvcc"
  | "wrapper"
  | "unknown";

export function identify_compiler(compiler_name: string): Compiler;
