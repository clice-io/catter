/// Inputs too many to check in: `t.disk.write(generate.sources(100))`.

/// `count` sources under src/, half C and half C++, each refusing to
/// compile without CATTER_TEST defined, as a build system defines it.
export function sources(count: number): Record<string, string> {
    const guard = '#ifndef CATTER_TEST\n#error "CATTER_TEST must be defined"\n#endif\n';
    return Object.fromEntries(
        Array.from({ length: count }, (_, i) => [
            `src/f${String(i).padStart(3, "0")}.${i % 2 === 0 ? "c" : "cc"}`,
            `${guard}int f${i}(void) { return ${i}; }\n`,
        ]),
    );
}
