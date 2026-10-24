# catter

Handlers for [catter](https://github.com/clice-io/catter) in Python, with the standard library alone: a handler is told of the commands a build under catter starts, and may decide what catter does with each.

```python
import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "act-on-me" in command.args:
            return catter.Drop()
        return None


catter.serve(Handler())
```

```sh
catter --handler "python handler.py" -- make
```

The protocol and the SDK are described in catter's documentation (`docs/en/guide/handlers.md`).
