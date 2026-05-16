import {
  time_monotonic_ms,
  time_monotonic_us,
  time_unix_ms,
  time_unix_seconds,
  time_unix_us,
} from "catter-c";

export {};

export type DurationUnit = "ns" | "us" | "ms" | "s" | "min" | "h" | "d";

const MS_PER_UNIT: Record<DurationUnit, number> = {
  ns: 1 / 1_000_000,
  us: 1 / 1_000,
  ms: 1,
  s: 1_000,
  min: 60_000,
  h: 3_600_000,
  d: 86_400_000,
};

/**
 * Returns the current Unix timestamp in milliseconds.
 *
 * This follows the same unit as `Date.now()`.
 */
export function now(): number {
  return time_unix_ms();
}

/**
 * Returns the current Unix timestamp in milliseconds.
 */
export function unixMs(): number {
  return time_unix_ms();
}

/**
 * Returns the current Unix timestamp in microseconds.
 */
export function unixUs(): number {
  return time_unix_us();
}

/**
 * Returns the current Unix timestamp in seconds.
 */
export function unixSeconds(): number {
  return time_unix_seconds();
}

/**
 * Returns a monotonic timestamp in milliseconds.
 *
 * Use this for measuring elapsed time instead of wall-clock time.
 */
export function monotonicMs(): number {
  return time_monotonic_ms();
}

/**
 * Returns a monotonic timestamp in microseconds.
 *
 * Use this for fine-grained elapsed-time measurement.
 */
export function monotonicUs(): number {
  return time_monotonic_us();
}

/**
 * Converts a duration between time units.
 */
export function convert(
  value: number,
  from: DurationUnit,
  to: DurationUnit,
): number {
  return (value * MS_PER_UNIT[from]) / MS_PER_UNIT[to];
}

/**
 * Converts a duration to milliseconds.
 */
export function toMs(value: number, from: DurationUnit): number {
  return convert(value, from, "ms");
}

/**
 * Converts a millisecond duration to another unit.
 */
export function fromMs(value: number, to: DurationUnit): number {
  return convert(value, "ms", to);
}

/**
 * Creates a millisecond duration from nanoseconds.
 */
export function ns(value: number): number {
  return toMs(value, "ns");
}

/**
 * Creates a millisecond duration from microseconds.
 */
export function us(value: number): number {
  return toMs(value, "us");
}

/**
 * Keeps a millisecond duration explicit at call sites.
 */
export function ms(value: number): number {
  return value;
}

/**
 * Creates a millisecond duration from seconds.
 */
export function seconds(value: number): number {
  return toMs(value, "s");
}

/**
 * Creates a millisecond duration from minutes.
 */
export function minutes(value: number): number {
  return toMs(value, "min");
}

/**
 * Creates a millisecond duration from hours.
 */
export function hours(value: number): number {
  return toMs(value, "h");
}

/**
 * Creates a millisecond duration from days.
 */
export function days(value: number): number {
  return toMs(value, "d");
}

/**
 * Returns elapsed milliseconds from a monotonic start timestamp.
 */
export function elapsedMs(startMs: number, endMs = monotonicMs()): number {
  return endMs - startMs;
}

/**
 * Returns elapsed time from a monotonic start timestamp in the requested unit.
 */
export function elapsed(
  startMs: number,
  unit: DurationUnit = "ms",
  endMs = monotonicMs(),
): number {
  return fromMs(elapsedMs(startMs, endMs), unit);
}
