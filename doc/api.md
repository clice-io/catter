```typescript
// ------------------------------------------------------------
// Internal API, not for plugin developers
// ------------------------------------------------------------
const ActionKind = ['skip', 'drop', 'abort', 'modify'] as const;

/**
 * @type skip - skip this command, but execute the original command
 * @type drop - drop this command, do not execute the original command
 * @type abort - abort the whole execution, and return an error
 * @type modify - modify this command, and execute the modified command
 */
type ActionType = (typeof ActionKind)[number];

type Action = {
    // for modify
    data?: CommandData;
    type: ActionType;
};

const EventKind = ['finish', 'output'] as const;
type EventType = (typeof EventKind)[number];

type ExecutionEvent = {
    // for output
    stdout?: string;
    stderr?: string;
    code: number;
    type: EventType;
};

type CatterRuntime = {
    supportActions: ActionType[];
    supportEvents: EventType[];
    // eslogger: only in mac
    // env: eg. CC=catter-proxy, then proxy report this cmd
    type: 'inject' | 'eslogger' | 'env';
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
    scriptPath: string;
    scriptArgs: string[];
    buildSystemCommand: string[];
    runtime: CatterRuntime;
    options: {
        log: boolean;
    };
    isScriptSupported: boolean;
};

type CatterErr = {
    //...
};

/**
 * @field parent - When supportParentId is true at runtime, this field is the ID of the parent command that generated this command; otherwise, this field is undefined.
 * @field env - the environment variables of this command, in the format of ["KEY=VALUE", ...]
 */
type CommandData = {
    cwd: string;
    exe: string;
    argv: string[];
    env: string[];
    runtime: CatterRuntime;
    parent?: number;
};

export function onStart(cb: (config: CatterConfig) => CatterConfig): void;
export function onFinish(cb: () => void): void;
export function onCommand(cb: (id: number, data: CommandData | CatterErr) => Action): void;
export function onExecution(cb: (id: number, event: ExecutionEvent) => void): void;


// ------------------------------------------------------------
// Plugin API, for plugin developers
// ------------------------------------------------------------

import { onStart, onFinish, onCommand, onExecution } from 'catter-c';

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

export function register(service: CatterService) {
    onStart(service.onStart);
    onFinish(service.onFinish);
    onCommand(service.onCommand);
    onExecution(service.onExecution);
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

register(new MyCatterPlugin());
```
