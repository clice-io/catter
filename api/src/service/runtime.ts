import type {
  Action,
  CatterConfig,
  CommandCaptureResult,
  CommandData,
  ProcessResult,
} from "catter-c";

export type MaybePromise<T> = T | Promise<T>;

export type CommandHandlerResult = Action | void;

export type ServiceStartHandler = (
  config: CatterConfig,
) => MaybePromise<CatterConfig | void>;

export type ServiceFinishHandler = (
  result: ProcessResult,
) => MaybePromise<void>;

export type LegacyCommandHandler = (
  id: number,
  data: CommandCaptureResult,
) => MaybePromise<Action>;

export type ContextCommandHandler = (
  ctx: CommandContext,
) => MaybePromise<CommandHandlerResult>;

export type LegacyExecutionHandler = (
  id: number,
  result: ProcessResult,
) => MaybePromise<void>;

export type ContextExecutionHandler = (
  ctx: ExecutionContext,
) => MaybePromise<void>;

export interface CommandContext {
  readonly id: number;
  readonly capture: CommandCaptureResult;
  readonly action: Action;
  readonly stopped: boolean;

  skip(): void;
  drop(): void;
  abort(): void;
  modify(data: CommandData): void;
  setAction(action: Action): void;
  ignoreDescendants(): void;
  stopPropagation(): void;
}

export interface ExecutionContext {
  readonly id: number;
  readonly result: ProcessResult;
  readonly stopped: boolean;

  stopPropagation(): void;
}

export interface CatterService {
  onStart?: ServiceStartHandler;
  onFinish?: ServiceFinishHandler;
  onCommand?: LegacyCommandHandler;
  onExecution?: LegacyExecutionHandler;
}

export interface CatterContextService {
  onStart?: ServiceStartHandler;
  onFinish?: ServiceFinishHandler;
  onCommand?: ContextCommandHandler;
  onExecution?: ContextExecutionHandler;
}

export interface CatterServiceAdapter {
  asService: () => RegisterableService;
}

export type RegisterableService =
  | CatterService
  | CatterContextService
  | CatterServiceAdapter;

type RuntimeService = {
  onStart?: ServiceStartHandler;
  onFinish?: ServiceFinishHandler;
  onCommand?: ContextCommandHandler;
  onExecution?: ContextExecutionHandler;
};

class RuntimeCommandContext implements CommandContext {
  private currentAction: Action = { type: "skip" };
  private propagationStopped = false;
  private actionSet = false;

  constructor(
    private readonly owner: ServiceRuntime,
    readonly id: number,
    readonly capture: CommandCaptureResult,
  ) {}

  get action(): Action {
    return this.currentAction;
  }

  get stopped(): boolean {
    return this.propagationStopped;
  }

  skip(): void {
    this.actionSet = true;
    this.currentAction = { type: "skip" };
  }

  drop(): void {
    this.actionSet = true;
    this.currentAction = { type: "drop" };
  }

  abort(): void {
    this.actionSet = true;
    this.currentAction = { type: "abort" };
  }

  modify(data: CommandData): void {
    this.actionSet = true;
    this.currentAction = { type: "modify", data };
  }

  setAction(action: Action): void {
    this.actionSet = true;
    this.currentAction = action;
  }

  ignoreDescendants(): void {
    this.owner.ignoreDescendantsOf(this.id);
  }

  stopPropagation(): void {
    this.propagationStopped = true;
  }

  hasAction(): boolean {
    return this.actionSet;
  }
}

class ParallelCommandContext implements CommandContext {
  private currentAction: Action = { type: "skip" };
  private propagationStopped = false;
  private actionSet = false;

  constructor(
    private readonly parent: CommandContext,
    readonly id: number,
    readonly capture: CommandCaptureResult,
  ) {}

  get action(): Action {
    return this.currentAction;
  }

  get stopped(): boolean {
    return this.propagationStopped;
  }

  skip(): void {
    this.actionSet = true;
    this.currentAction = { type: "skip" };
  }

  drop(): void {
    this.actionSet = true;
    this.currentAction = { type: "drop" };
  }

  abort(): void {
    this.actionSet = true;
    this.currentAction = { type: "abort" };
  }

  modify(data: CommandData): void {
    this.actionSet = true;
    this.currentAction = { type: "modify", data };
  }

  setAction(action: Action): void {
    this.actionSet = true;
    this.currentAction = action;
  }

  ignoreDescendants(): void {
    this.parent.ignoreDescendants();
  }

  stopPropagation(): void {
    this.propagationStopped = true;
  }

  hasAction(): boolean {
    return this.actionSet;
  }
}

class RuntimeExecutionContext implements ExecutionContext {
  private propagationStopped = false;

  constructor(
    readonly id: number,
    readonly result: ProcessResult,
  ) {}

  get stopped(): boolean {
    return this.propagationStopped;
  }

  stopPropagation(): void {
    this.propagationStopped = true;
  }
}

export class ServiceRuntime {
  private readonly services: RuntimeService[] = [];
  private readonly commandParentIds = new Map<number, number | undefined>();
  private readonly ignoredCommandIds = new Set<number>();

  use(service: RegisterableService): this {
    this.services.push(adaptService(service));
    return this;
  }

  async start(config: CatterConfig): Promise<CatterConfig> {
    let current = config;
    for (const service of this.services) {
      const next = await service.onStart?.(current);
      if (next !== undefined) {
        current = next;
      }
    }
    return current;
  }

  async finish(result: ProcessResult): Promise<void> {
    for (const service of this.services) {
      await service.onFinish?.(result);
    }
  }

  async command(id: number, data: CommandCaptureResult): Promise<Action> {
    this.rememberCommand(id, data.success ? data.data.parent : undefined);

    const ctx = new RuntimeCommandContext(this, id, data);
    if (this.hasIgnoredAncestor(id)) {
      return ctx.action;
    }

    await this.dispatchCommand(ctx);
    return ctx.action;
  }

  async dispatchCommand(ctx: CommandContext): Promise<void> {
    for (const service of this.services) {
      const action = await service.onCommand?.(ctx);
      if (action !== undefined) {
        ctx.setAction(action);
      }
      if (ctx.stopped) {
        break;
      }
    }
  }

  async execution(id: number, result: ProcessResult): Promise<void> {
    if (this.hasCommand(id) && this.hasIgnoredAncestor(id)) {
      return;
    }

    const ctx = new RuntimeExecutionContext(id, result);
    for (const service of this.services) {
      await service.onExecution?.(ctx);
      if (ctx.stopped) {
        break;
      }
    }
  }

  rememberCommand(id: number, parentId?: number): void {
    this.commandParentIds.set(id, parentId);
  }

  hasCommand(id: number): boolean {
    return this.commandParentIds.has(id);
  }

  commandParentId(id: number): number | undefined {
    return this.commandParentIds.get(id);
  }

  ignoreDescendantsOf(id: number): void {
    this.ignoredCommandIds.add(id);
  }

  isIgnored(id: number): boolean {
    return this.ignoredCommandIds.has(id);
  }

  hasIgnoredAncestor(id: number): boolean {
    if (!this.hasCommand(id)) {
      return false;
    }

    let parentId = this.commandParentId(id);
    while (parentId !== undefined) {
      if (this.ignoredCommandIds.has(parentId)) {
        return true;
      }
      parentId = this.commandParentId(parentId);
    }

    return false;
  }

  asService(): CatterService {
    return {
      onStart: (config) => this.start(config),
      onFinish: (result) => this.finish(result),
      onCommand: (id, data) => this.command(id, data),
      onExecution: (id, result) => this.execution(id, result),
    };
  }
}

export function create(service: CatterContextService): CatterContextService {
  return service;
}

export function pipeline(...services: RegisterableService[]): CatterService {
  const runtime = new ServiceRuntime();
  for (const service of services) {
    runtime.use(service);
  }
  return runtime.asService();
}

export function parallel(
  ...services: RegisterableService[]
): CatterContextService {
  const runtimeServices = services.map((service) => adaptService(service));

  return {
    onStart: async (config) => {
      const configs = await Promise.all(
        runtimeServices.map((service) => service.onStart?.(config)),
      );
      return consistentResult(configs, config, "onStart");
    },
    onFinish: async (result) => {
      await Promise.all(
        runtimeServices.map((service) => service.onFinish?.(result)),
      );
    },
    onCommand: async (ctx) => {
      return await dispatchParallelCommand(runtimeServices, ctx);
    },
    onExecution: async (ctx) => {
      await Promise.all(
        runtimeServices.map((service) => service.onExecution?.(ctx)),
      );
    },
  };
}

function hasServiceAdapter(
  service: RegisterableService,
): service is CatterServiceAdapter {
  return "asService" in service && typeof service.asService === "function";
}

function adaptService(service: RegisterableService): RuntimeService {
  if (hasServiceAdapter(service)) {
    return adaptService(service.asService());
  }

  return {
    onStart: service.onStart,
    onFinish: service.onFinish,
    onCommand: service.onCommand
      ? (ctx) => callCommandHandler(service.onCommand!, ctx)
      : undefined,
    onExecution: service.onExecution
      ? (ctx) => callExecutionHandler(service.onExecution!, ctx)
      : undefined,
  };
}

async function callCommandHandler(
  handler: LegacyCommandHandler | ContextCommandHandler,
  ctx: CommandContext,
): Promise<CommandHandlerResult> {
  if (handler.length <= 1) {
    return (handler as ContextCommandHandler)(ctx);
  }
  return (handler as LegacyCommandHandler)(ctx.id, ctx.capture);
}

async function callExecutionHandler(
  handler: LegacyExecutionHandler | ContextExecutionHandler,
  ctx: ExecutionContext,
): Promise<void> {
  if (handler.length <= 1) {
    return (handler as ContextExecutionHandler)(ctx);
  }
  return (handler as LegacyExecutionHandler)(ctx.id, ctx.result);
}

async function dispatchParallelCommand(
  services: RuntimeService[],
  parent: CommandContext,
): Promise<Action | undefined> {
  const outputs = await Promise.all(
    services.map(async (service) => {
      if (!service.onCommand) {
        return undefined;
      }

      const ctx = new ParallelCommandContext(parent, parent.id, parent.capture);
      const returned = await service.onCommand(ctx);
      if (returned !== undefined) {
        ctx.setAction(returned);
      }
      if (ctx.stopped) {
        parent.stopPropagation();
      }
      if (!ctx.hasAction()) {
        return undefined;
      }
      return ctx.action;
    }),
  );

  const actions = outputs.filter(
    (action): action is Action => action !== undefined,
  );
  if (actions.length > 1) {
    throw new Error(
      `service.parallel expected at most one action result, got ${actions.length}`,
    );
  }

  return actions[0];
}

function consistentResult<T>(
  values: Array<T | void>,
  fallback: T,
  hookName: string,
): T {
  const results = values.filter((value): value is T => value !== undefined);
  if (results.length === 0) {
    return fallback;
  }

  const first = results[0];
  const firstJson = canonicalJson(first);
  for (const result of results.slice(1)) {
    if (canonicalJson(result) !== firstJson) {
      throw new Error(
        `service.parallel expected identical ${hookName} results, got differing results`,
      );
    }
  }

  return first;
}

function canonicalJson(value: unknown): string {
  return JSON.stringify(sortJsonValue(value));
}

function sortJsonValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map(sortJsonValue);
  }

  if (value === null || typeof value !== "object") {
    return value;
  }

  const object = value as Record<string, unknown>;
  return Object.fromEntries(
    Object.keys(object)
      .sort()
      .map((key) => [key, sortJsonValue(object[key])]),
  );
}
