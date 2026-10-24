import { println } from "catter/io";
import { onCommand } from "catter/service";
import { message } from "./lib/message.js";

println(message);
onCommand(() => {});
