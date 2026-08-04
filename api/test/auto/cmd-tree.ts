import * as scripts from "catter/scripts";
import * as service from "catter/service";

const cmdTree = scripts.cmdTree();
service.register(cmdTree);
