# Build Profiling

> [!WARNING]
> This feature is planned but not yet implemented.

## Concept

Build profiling captures per-process timing data during a build:

- Process start time and duration
- Parent-child relationships
- CPU utilization

This data is rendered as a visual timeline (Gantt-chart style) in a browser, allowing developers to diagnose build performance issues.

## Planned Capabilities

- **Identify serialization bottlenecks** -- Find stages where the build runs single-threaded despite available parallelism.
- **Measure actual vs. theoretical parallelism** -- Compare the observed concurrency level against the number of available cores.
- **Find slow compilation units** -- Pinpoint individual source files that take disproportionately long to compile.
- **Debug build system configuration** -- Detect misconfigured dependencies, unnecessary serialization, or suboptimal job scheduling.
