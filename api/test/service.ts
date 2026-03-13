import { debug, service } from "catter";

const serviceArg = "--from-service";

let outputEventSeen = false;
let finishEventSeen = false;
let commandErrorBranchSeen = false;

service.register({
	onStart(config) {
		debug.assertThrow(config.scriptPath === "script.ts");
		debug.assertThrow(config.scriptArgs.length === 2);
		debug.assertThrow(config.options.log);

		return {
			...config,
			scriptArgs: [...config.scriptArgs, serviceArg],
			options: {
				...config.options,
				log: false,
			},
			isScriptSupported: false,
		};
	},

	onFinish() {
		debug.assertThrow(outputEventSeen);
		debug.assertThrow(finishEventSeen);
		debug.assertThrow(commandErrorBranchSeen);
	},

	onCommand(id, data) {
		debug.assertThrow(id === 7);

		if ("msg" in data) {
			debug.assertThrow(data.msg === "spawn failed");
			commandErrorBranchSeen = true;
			return { type: "skip" };
		}

		debug.assertThrow(data.cwd === "/tmp");
		debug.assertThrow(data.exe === "clang++");
		debug.assertThrow(data.argv.length === 3);
		debug.assertThrow(data.argv[2] === "-c");
		debug.assertThrow(data.parent === 41);

		return {
			type: "modify",
			data: {
				...data,
				argv: [...data.argv, serviceArg],
			},
		};
	},

	onExecution(id, event) {
		debug.assertThrow(id === 7);

		if (event.type === "output") {
			debug.assertThrow(event.stdout === "hello from stdout");
			debug.assertThrow(event.stderr === "hello from stderr");
			outputEventSeen = true;
			return;
		}

		debug.assertThrow(event.type === "finish");
		debug.assertThrow(event.code === 0);
		finishEventSeen = true;
	},
});
