import { scripts, service } from "catter";

const cmdTree = scripts.cmdTree();
service.register(cmdTree);
