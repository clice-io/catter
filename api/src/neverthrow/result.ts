export type InferOkTypes<R> = R extends Result<infer T, unknown> ? T : never;
export type InferErrTypes<R> = R extends Result<unknown, infer E> ? E : never;

export type Result<T, E> = Ok<T, E> | Err<T, E>;

export function ok<T, E = never>(value: T): Ok<T, E>;
export function ok<T extends void = void, E = never>(value: void): Ok<void, E>;
export function ok<T, E = never>(value: T): Ok<T, E> {
    return new Ok(value);
}

export function err<T = never, E extends string = string>(err: E): Err<T, E>;
export function err<T = never, E = unknown>(err: E): Err<T, E>;
export function err<T = never, E extends void = void>(err: void): Err<T, void>;
export function err<T = never, E = unknown>(err: E): Err<T, E> {
    return new Err(err);
}

export interface IResult<T, E> {
    /**
     * Used to check if a `Result` is an `OK`
     *
     * @returns `true` if the result is an `OK` variant of Result
     */
    isOk(): this is Ok<T, E>;

    /**
     * Used to check if a `Result` is an `Err`
     *
     * @returns `true` if the result is an `Err` variant of Result
     */
    isErr(): this is Err<T, E>;

    /**
     * Maps a `Result<T, E>` to `Result<U, E>`
     * by applying a function to a contained `Ok` value, leaving an `Err` value
     * untouched.
     *
     * @param f - The function to apply an `OK` value
     * @returns the result of applying `f` or an `Err` untouched
     */
    map<A>(f: (t: T) => A): Result<A, E>;

    /**
     * Maps a `Result<T, E>` to `Result<T, F>` by applying a function to a
     * contained `Err` value, leaving an `Ok` value untouched.
     *
     * This function can be used to pass through a successful result while
     * handling an error.
     *
     * @param f - A function to apply to the error `Err` value
     */
    mapErr<U>(f: (e: E) => U): Result<T, U>;

    /**
     * Similar to `map` Except you must return a new `Result`.
     *
     * This is useful for when you need to do a subsequent computation using the
     * inner `T` value, but that computation might fail.
     * Additionally, `andThen` is really useful as a tool to flatten a
     * `Result<Result<A, E2>, E1>` into a `Result<A, E2>` (see example below).
     *
     * @param f - The function to apply to the current value
     */
    andThen<R extends Result<unknown, unknown>>(
        f: (t: T) => R,
    ): Result<InferOkTypes<R>, InferErrTypes<R> | E>;
    andThen<U, F>(f: (t: T) => Result<U, F>): Result<U, E | F>;

    /**
     * This "tee"s the current value to an passed-in computation such as side
     * effect functions but still returns the same current value as the result.
     *
     * This is useful when you want to pass the current result to your side-track
     * work such as logging but want to continue main-track work after that.
     * This method does not care about the result of the passed in computation.
     *
     * @param f - The function to apply to the current value
     */
    andTee(f: (t: T) => unknown): Result<T, E>;

    /**
     * This "tee"s the current `Err` value to an passed-in computation such as side
     * effect functions but still returns the same `Err` value as the result.
     *
     * This is useful when you want to pass the current `Err` value to your side-track
     * work such as logging but want to continue error-track work after that.
     * This method does not care about the result of the passed in computation.
     *
     * @param f - The function to apply to the current `Err` value
     */
    orTee(f: (t: E) => unknown): Result<T, E>;

    /**
     * Similar to `andTee` except error result of the computation will be passed
     * to the downstream in case of an error.
     *
     * This version is useful when you want to make side-effects but in case of an
     * error, you want to pass the error to the downstream.
     *
     * @param f - The function to apply to the current value
     */
    andThrough<R extends Result<unknown, unknown>>(f: (t: T) => R): Result<T, InferErrTypes<R> | E>;
    andThrough<F>(f: (t: T) => Result<unknown, F>): Result<T, E | F>;

    /**
     * Takes an `Err` value and maps it to a `Result<T, SomeNewType>`.
     *
     * This is useful for error recovery.
     *
     *
     * @param f - A function to apply to an `Err` value, leaving `Ok` values
     * untouched.
     */
    orElse<R extends Result<unknown, unknown>>(
        f: (e: E) => R,
    ): Result<InferOkTypes<R> | T, InferErrTypes<R>>;
    orElse<U, A>(f: (e: E) => Result<U, A>): Result<U | T, A>;

    /**
     * Unwrap the `Ok` value, or return the default if there is an `Err`
     *
     * @param v - The default value to return if there is an `Err`
     */
    unwrapOr<A>(v: A): T | A;

    /**
     *
     * Given 2 functions (one for the `Ok` variant and one for the `Err` variant)
     * execute the function that matches the `Result` variant.
     *
     * Match callbacks do not necessitate to return a `Result`, however you can
     * return a `Result` if you want to.
     *
     * `match` is like chaining `map` and `mapErr`, with the distinction that
     * with `match` both functions must have the same return type.
     *
     * @param ok - Callback for the `Ok` variant.
     * @param err - Callback for the `Err` variant.
     */
    match<A, B = A>(ok: (t: T) => A, err: (e: E) => B): A | B;

    /**
     * @deprecated will be removed in 9.0.0.
     *
     * You can use `safeTry` without this method.
     * @example
     * ```typescript
     * safeTry(function* () {
     *   const okValue = yield* yourResult
     * })
     * ```
     * Emulates Rust's `?` operator in `safeTry`'s body. See also `safeTry`.
     */
    safeUnwrap(): Generator<Err<never, E>, T>;
}

export class Ok<T, E> implements IResult<T, E> {
    constructor(readonly value: T) {}

    isOk(): this is Ok<T, E> {
        return true;
    }

    isErr(): this is Err<T, E> {
        return !this.isOk();
    }

    map<A>(f: (t: T) => A): Result<A, E> {
        return ok(f(this.value));
    }

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    mapErr<U>(_f: (e: E) => U): Result<T, U> {
        return ok(this.value);
    }

    andThen<R extends Result<unknown, unknown>>(
        f: (t: T) => R,
    ): Result<InferOkTypes<R>, InferErrTypes<R> | E>;
    andThen<U, F>(f: (t: T) => Result<U, F>): Result<U, E | F>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    andThen(f: any): any {
        return f(this.value);
    }

    andThrough<R extends Result<unknown, unknown>>(f: (t: T) => R): Result<T, InferErrTypes<R> | E>;
    andThrough<F>(f: (t: T) => Result<unknown, F>): Result<T, E | F>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    andThrough(f: any): any {
        return f(this.value).map((_value: unknown) => this.value);
    }

    andTee(f: (t: T) => unknown): Result<T, E> {
        try {
            f(this.value);
        } catch (e) {
            // Tee doesn't care about the error
        }
        return ok<T, E>(this.value);
    }

    orTee(_f: (t: E) => unknown): Result<T, E> {
        return ok<T, E>(this.value);
    }

    orElse<R extends Result<unknown, unknown>>(
        _f: (e: E) => R,
    ): Result<InferOkTypes<R> | T, InferErrTypes<R>>;
    orElse<U, A>(_f: (e: E) => Result<U, A>): Result<U | T, A>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    orElse(_f: any): any {
        return ok(this.value);
    }

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    unwrapOr<A>(_v: A): T | A {
        return this.value;
    }

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    match<A, B = A>(ok: (t: T) => A, _err: (e: E) => B): A | B {
        return ok(this.value);
    }

    safeUnwrap(): Generator<Err<never, E>, T> {
        const value = this.value;
        /* eslint-disable-next-line require-yield */
        return (function* () {
            return value;
        })();
    }

    // eslint-disable-next-line @typescript-eslint/no-this-alias, require-yield
    *[Symbol.iterator](): Generator<Err<never, E>, T> {
        return this.value;
    }
}

export class Err<T, E> implements IResult<T, E> {
    constructor(readonly error: E) {}

    isOk(): this is Ok<T, E> {
        return false;
    }

    isErr(): this is Err<T, E> {
        return !this.isOk();
    }

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    map<A>(_f: (t: T) => A): Result<A, E> {
        return err(this.error);
    }

    mapErr<U>(f: (e: E) => U): Result<T, U> {
        return err(f(this.error));
    }

    andThrough<F>(_f: (t: T) => Result<unknown, F>): Result<T, E | F> {
        return err(this.error);
    }

    andTee(_f: (t: T) => unknown): Result<T, E> {
        return err(this.error);
    }

    orTee(f: (t: E) => unknown): Result<T, E> {
        try {
            f(this.error);
        } catch (e) {
            // Tee doesn't care about the error
        }
        return err<T, E>(this.error);
    }

    andThen<R extends Result<unknown, unknown>>(
        _f: (t: T) => R,
    ): Result<InferOkTypes<R>, InferErrTypes<R> | E>;
    andThen<U, F>(_f: (t: T) => Result<U, F>): Result<U, E | F>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    andThen(_f: any): any {
        return err(this.error);
    }

    orElse<R extends Result<unknown, unknown>>(
        f: (e: E) => R,
    ): Result<InferOkTypes<R> | T, InferErrTypes<R>>;
    orElse<U, A>(f: (e: E) => Result<U, A>): Result<U | T, A>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    orElse(f: any): any {
        return f(this.error);
    }

    unwrapOr<A>(v: A): T | A {
        return v;
    }

    match<A, B = A>(_ok: (t: T) => A, err: (e: E) => B): A | B {
        return err(this.error);
    }

    safeUnwrap(): Generator<Err<never, E>, T> {
        const error = this.error;
        return (function* () {
            yield err(error);

            throw new Error("Do not use this generator out of `safeTry`");
        })();
    }

    *[Symbol.iterator](): Generator<Err<never, E>, T> {
        // eslint-disable-next-line @typescript-eslint/no-this-alias
        const self = this;
        // @ts-expect-error -- This is structurally equivalent and safe
        yield self;
        // @ts-expect-error -- This is structurally equivalent and safe
        return self;
    }
}

/**
 * Wraps a function with a try catch, creating a new function with the same
 * arguments but returning `Ok` if successful, `Err` if the function throws
 *
 * @param fn - Function to wrap with ok on success or err on failure
 * @param errorFn - When an error is thrown, this will wrap the error result if provided
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function fromThrowable<Fn extends (...args: readonly any[]) => any, E>(
    fn: Fn,
    errorFn?: (e: unknown) => E,
): (...args: Parameters<Fn>) => Result<ReturnType<Fn>, E> {
    return (...args) => {
        try {
            const result = fn(...args);
            return ok(result);
        } catch (e) {
            return err(errorFn ? errorFn(e) : (e as E));
        }
    };
}
