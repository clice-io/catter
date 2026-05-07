import {
  service_on_command,
  service_on_execution,
  service_on_finish,
  service_on_start,
} from "catter-c";

import {
  ServiceRuntime,
  type CatterContextService,
  type CatterService,
  type CatterServiceAdapter,
  type ContextCommandHandler,
  type ContextExecutionHandler,
  type LegacyCommandHandler,
  type LegacyExecutionHandler,
  type RegisterableService,
  type ServiceFinishHandler,
  type ServiceStartHandler,
} from "./service/runtime.js";

export * from "./service/runtime.js";

export type {
  Action,
  ActionType,
  CatterConfig,
  CatterErr,
  CatterOutputMode,
  CatterRuntime,
  CommandCaptureResult,
  CommandData,
  ProcessResult,
} from "catter-c";

import type { ActionType } from "catter-c";

/**
 * Supported command actions.
 *
 * These string literals are the valid `type` values for {@link Action}.
 */
export const ActionKind = [
  "skip",
  "drop",
  "abort",
  "modify",
] as const satisfies readonly ActionType[];

const defaultRuntime = new ServiceRuntime();
let runtimeInstalled = false;

function installRuntime(): void {
  if (runtimeInstalled) {
    return;
  }

  runtimeInstalled = true;
  service_on_start((config) => defaultRuntime.start(config));
  service_on_finish((result) => defaultRuntime.finish(result));
  service_on_command((id, data) => defaultRuntime.command(id, data));
  service_on_execution((id, result) => defaultRuntime.execution(id, result));
}

/**
 * Registers a callback that can inspect and modify the runtime config before catter starts.
 */
export function onStart(cb: ServiceStartHandler): void {
  register({ onStart: cb });
}

/**
 * Registers a callback that runs after catter finishes.
 */
export function onFinish(cb: ServiceFinishHandler): void {
  register({ onFinish: cb });
}

/**
 * Registers a callback that handles each captured command.
 */
export function onCommand(cb: LegacyCommandHandler): void;
export function onCommand(cb: ContextCommandHandler): void;
export function onCommand(
  cb: LegacyCommandHandler | ContextCommandHandler,
): void {
  installRuntime();
  defaultRuntime.use({ onCommand: cb } as RegisterableService);
}

/**
 * Registers a callback that receives final process results for captured commands.
 */
export function onExecution(cb: LegacyExecutionHandler): void;
export function onExecution(cb: ContextExecutionHandler): void;
export function onExecution(
  cb: LegacyExecutionHandler | ContextExecutionHandler,
): void {
  installRuntime();
  defaultRuntime.use({ onExecution: cb } as RegisterableService);
}

/**
 * Registers a service with the default catter runtime.
 */
export function register(service: CatterService): void;
export function register(service: CatterContextService): void;
export function register(service: CatterServiceAdapter): void;
export function register(service: RegisterableService): void {
  installRuntime();
  defaultRuntime.use(service);
}
