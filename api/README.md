# Catter

Catter QuickJS runtime API types and editor hints.

This package is intended for code completion and type checking in scripts that
run inside the Catter runtime. It is not a Node.js runtime implementation of
Catter APIs.

## Install

```sh
npm install --save-dev catter
```

## Usage

```js
// @ts-check
import { debug, fs, service } from "catter";

debug.assertPrint(await fs.async.exists("build"));

service.register({
  async onStart(config) {
    return config;
  },
});
```
