```typescript

const ActionKind = ['skip', 'drop', 'abort', 'modify'] as const;

/**
 * @type skip - skip this command, but execute the original command
 * @type drop - drop this command, do not execute the original command
 * @type abort - abort the whole execution, and return an error
 * @type modify - modify this command, and execute the modified command
 */
type ActionType = (typeof ActionKind)[number];

type Action = {
    type: ActionType;
    // for modify
    data?: CommandData;
};

const EventKind = ['finish', 'output'] as const;
type EventType = (typeof EventKind)[number];

type ExecutionEvent = {
    type: EventType;
    code: number;
    // for output
    stdout?: string;
    stderr?: string;
};

type CatterRuntime = {
    // eslogger: only in mac
    // env: eg. CC=catter-proxy, then proxy report this cmd
    type: 'hook' | 'eslogger' | 'env';
    supportActions: ActionType[];
    supportEvents: EventType[];
    supportParentId: boolean;
};

/**
 * @field scriptArgs - the arguments of this script
 * @field scriptPath - the path of this script
 * @field buildSystemCommand - the command to execute this script in build system, eg. ['bazel', 'build', '//:target']
 * @field isScriptSupported - defaults to true, if false, catter will instantly abort the execution and return an error.
 * @field runtime - the runtime environment of this script, can be used to determine which actions and events are supported
 * @field options - the options of catter, can be used to enable some features of catter, eg. log
 */
type CatterConfig = {
    scriptArgs: string[];
    scriptPath: string;
    buildSystemCommand: string[];
    runtime: CatterRuntime;
    isScriptSupported: boolean;
    options: {
        log: boolean;
    };
};

type CatterErr = {
    //...
};

/**
 * @field parent - When supportParentId is true at runtime, this field is the ID of the parent command that generated this command; otherwise, this field is undefined.
 */
type CommandData = {
    cwd: string;
    exe: string;
    argv: string[];
    env: Map<string, string>;
    parent?: number;
    runtime: CatterRuntime;
};

type ServiceRegister = {
    onStart: (cb: (config: CatterConfig) => CatterConfig) => void;
    onFinish: (cb: () => void) => void;
    onCommand: (cb: (id: number, data: CommandData | CatterErr) => Action) => void;
    onExecution: (cb: (id: number, event: ExecutionEvent) => void) => void;
};

declare let serviceRegister: ServiceRegister;

/**
 * @method onStart - called when catter start, can modify config
 * @method onFinish - called when catter finish
 * @method onCommand - called when a command being captured
 *    @param onCommand.id - a unique identifier for this command, can be used to correlate with onExecution
 *    @param onCommand.data - the data of this command, if there is an error during capturing, this will be a CatterErr object
 * @method onExecution - called when a command being executed, can listen on its output and finish event.
 *   @param onExecution.id - the unique identifier for this command, same as onCommand
 *   @param onExecution.event - the event of this command, if there is an error during execution, the code field will be non-zero and stdout/stderr may be undefined
 */
interface CatterService {
    onStart: (config: CatterConfig) => CatterConfig;
    onFinish: () => void;
    onCommand: (id: number, data: CommandData | CatterErr) => Action;
    onExecution: (id: number, event: ExecutionEvent) => void;
}

function registerService(service: CatterService) {
    serviceRegister.onStart(service.onStart);
    serviceRegister.onFinish(service.onFinish);
    serviceRegister.onCommand(service.onCommand);
    serviceRegister.onExecution(service.onExecution);
}

// ------------------------------------------------------------
// Example
// ------------------------------------------------------------
class MyCatterPlugin implements CatterService {
    dataMap: Map<number, CommandData | CatterErr> = new Map();
    eventMap: Map<number, ExecutionEvent> = new Map();
    onStart(config: CatterConfig): CatterConfig {
        // modify config
        return config;
    }

    onFinish(): void {
        for (const id of this.dataMap.keys()) {
            const data = this.dataMap.get(id);
            const event = this.eventMap.get(id);
            console.log(`Command ${id} data:`, data);
            console.log(`Command ${id} event:`, event);
        }
    }

    onCommand(id: number, data: CommandData | CatterErr): Action {
        this.dataMap.set(id, data);
        return { type: 'skip' };
    }

    onExecution(id: number, event: ExecutionEvent): void {
        this.eventMap.set(id, event);
    }
}

registerService(new MyCatterPlugin());
```
