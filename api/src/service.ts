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
 */
export const ActionKind = ["skip", "drop", "abort", "modify"] as const;

const _ActionKindTypeCheck: (typeof ActionKind)[number] = {} as ActionType;

/**
 * Supported command actions.
 */
export const EventKind = ["finish", "output"] as const;

const _EventKindTypeCheck: (typeof EventKind)[number] = {} as EventType;

/**
 * Callback group for subscribing to catter lifecycle and command events.
 */
export interface CatterService {
  /**
   * Called before catter starts processing commands.
   *
   * Return the config to be used for the current run.
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
 * @param cb - Callback invoked with the current config.
 */
export function onStart(cb: (config: CatterConfig) => CatterConfig): void {
  service_on_start(cb);
}

/**
 * Registers a callback that runs after catter finishes.
 *
 * @param cb - Callback invoked once at shutdown.
 */
export function onFinish(cb: () => void): void {
  service_on_finish(cb);
}

/**
 * Registers a callback that handles each captured command.
 *
 * @param cb - Callback invoked for each captured command.
 */
export function onCommand(
  cb: (id: number, data: CommandData | CatterErr) => Action,
): void {
  service_on_command(cb);
}

/**
 * Registers a callback that receives execution events for captured commands.
 *
 * @param cb - Callback invoked for command output and completion events.
 */
export function onExecution(
  cb: (id: number, event: ExecutionEvent) => void,
): void {
  service_on_execution(cb);
}

/**
 * Registers a full set of catter service callbacks in one call.
 *
 * @param service - Service callbacks for startup, shutdown, command capture, and command execution.
 *
 * @remarks
 * This is equivalent to calling {@link onStart}, {@link onFinish}, {@link onCommand},
 * and {@link onExecution} with the corresponding callbacks from `service`.
 */
export function register(service: CatterService): void {
  onStart(service.onStart);
  onFinish(service.onFinish);
  onCommand(service.onCommand);
  onExecution(service.onExecution);
}
