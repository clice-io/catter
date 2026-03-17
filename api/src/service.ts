import {
  service_on_command,
  service_on_execution,
  service_on_finish,
  service_on_start,
} from "catter-c";

export type {
  Action,
  ActionType,
  CatterConfig,
  CatterErr,
  CatterRuntime,
  CommandData,
  EventType,
  ExecutionEvent,
} from "catter-c";

import type {
  Action,
  ActionType,
  CatterConfig,
  CatterErr,
  CatterRuntime,
  CommandData,
  EventType,
  ExecutionEvent,
} from "catter-c";

/**
 * Supported command actions.
 *
 * These string literals are the valid `type` values for {@link Action}.
 *
 * @example
 * ```typescript
 * const kind = ActionKind[0]; // "skip"
 * ```
 */
export const ActionKind = ["skip", "drop", "abort", "modify"] as const;

const _ActionKindTypeCheck: (typeof ActionKind)[number] = {} as ActionType;

/**
 * Supported execution event kinds.
 *
 * These string literals match the `type` field emitted in {@link ExecutionEvent}.
 *
 * @example
 * ```typescript
 * const isOutputEvent = EventKind.includes("output");
 * ```
 */
export const EventKind = ["finish", "output"] as const;

const _EventKindTypeCheck: (typeof EventKind)[number] = {} as EventType;

/**
 * Callback group for subscribing to catter lifecycle and command events.
 *
 * @example
 * ```typescript
 * register({
 *   onStart(config) {
 *     return config;
 *   },
 *   onFinish() {},
 *   onCommand() {
 *     return { type: "skip" };
 *   },
 *   onExecution() {},
 * });
 * ```
 */
export interface CatterService {
  /**
   * Called before catter starts processing commands.
   *
   * Return the config to be used for the current run.
   *
   * @param config - The runtime configuration prepared by catter for this script invocation.
   * @returns The configuration object that catter should use for the rest of the run.
   */
  onStart: (config: CatterConfig) => CatterConfig;

  /**
   * Called after catter finishes processing.
   */
  onFinish: () => void;

  /**
   * Called when catter captures a command.
   *
   * @param id - Unique command identifier that can be correlated with execution events.
   * @param data - Captured command data, or a {@link CatterErr} when capture fails.
   */
  onCommand: (id: number, data: CommandData | CatterErr) => Action;

  /**
   * Called when a captured command emits execution events.
   *
   * @param id - Unique command identifier, matching the value passed to {@link CatterService.onCommand}.
   * @param event - Execution event payload for output or completion.
   */
  onExecution: (id: number, event: ExecutionEvent) => void;
}

/**
 * Registers a callback that can inspect and modify the runtime config before catter starts.
 *
 * @param cb - Callback that receives the current {@link CatterConfig} and returns the config to keep using.
 *
 * @example
 * ```typescript
 * onStart((config) => ({
 *   ...config,
 *   options: {
 *     ...config.options,
 *     log: false,
 *   },
 * }));
 * ```
 */
export function onStart(cb: (config: CatterConfig) => CatterConfig): void {
  service_on_start(cb);
}

/**
 * Registers a callback that runs after catter finishes.
 *
 * @param cb - Callback invoked once when the current catter run is shutting down.
 *
 * @example
 * ```typescript
 * onFinish(() => {
 *   println("build interception finished");
 * });
 * ```
 */
export function onFinish(cb: () => void): void {
  service_on_finish(cb);
}

/**
 * Registers a callback that handles each captured command.
 *
 * @param cb - Callback invoked for each command. The first argument is the stable command ID, and the second is either the captured command payload or a capture error.
 *
 * @example
 * ```typescript
 * onCommand((id, data) => {
 *   if ("msg" in data) {
 *     return { type: "skip" };
 *   }
 *   return { type: "modify", data: { ...data, argv: [...data.argv, "--verbose"] } };
 * });
 * ```
 */
export function onCommand(
  cb: (id: number, data: CommandData | CatterErr) => Action,
): void {
  service_on_command(cb);
}

/**
 * Registers a callback that receives execution events for captured commands.
 *
 * @param cb - Callback invoked with the command ID and each emitted execution event, such as process output or exit.
 *
 * @example
 * ```typescript
 * onExecution((id, event) => {
 *   if (event.type === "finish") {
 *     println("command " + id + " exited with " + event.code);
 *   }
 * });
 * ```
 */
export function onExecution(
  cb: (id: number, event: ExecutionEvent) => void,
): void {
  service_on_execution(cb);
}

/**
 * Registers a full set of catter service callbacks in one call.
 *
 * @param service - The callback collection that catter should subscribe to for this run.
 *
 * @remarks
 * This is equivalent to calling {@link onStart}, {@link onFinish}, {@link onCommand},
 * and {@link onExecution} with the corresponding callbacks from `service`.
 *
 * @example
 * ```typescript
 * register({
 *   onStart(config) {
 *     return config;
 *   },
 *   onFinish() {},
 *   onCommand(id, data) {
 *     return { type: "skip" };
 *   },
 *   onExecution() {},
 * });
 * ```
 */
export function register(service: CatterService): void {
  onStart(service.onStart);
  onFinish(service.onFinish);
  onCommand(service.onCommand);
  onExecution(service.onExecution);
}
