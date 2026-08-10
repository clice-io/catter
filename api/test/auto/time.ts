import { assertThrow } from "catter/debug";
import {
  convert,
  days,
  elapsed,
  elapsedMs,
  fromMs,
  hours,
  minutes,
  monotonicMs,
  monotonicUs,
  ms,
  now,
  ns,
  seconds,
  toMs,
  unixMs,
  unixSeconds,
  unixUs,
  us,
} from "catter/time";

const before = Date.now();
const nowMs = now();
const after = Date.now();

assertThrow(nowMs >= before - 1_000);
assertThrow(nowMs <= after + 1_000);
assertThrow(unixMs() >= before - 1_000);
assertThrow(unixUs() >= (before - 1_000) * 1_000);
assertThrow(Math.abs(unixSeconds() * 1_000 - nowMs) < 2_000);

const monotonicA = monotonicMs();
const monotonicB = monotonicMs();
assertThrow(monotonicB >= monotonicA);

const monotonicUsA = monotonicUs();
const monotonicUsB = monotonicUs();
assertThrow(monotonicUsB >= monotonicUsA);

assertThrow(convert(1, "s", "ms") === 1_000);
assertThrow(convert(2, "min", "s") === 120);
assertThrow(convert(1, "h", "min") === 60);
assertThrow(toMs(1, "d") === 86_400_000);
assertThrow(fromMs(1_500, "s") === 1.5);

assertThrow(ns(1_000_000) === 1);
assertThrow(us(1_000) === 1);
assertThrow(ms(9) === 9);
assertThrow(seconds(2) === 2_000);
assertThrow(minutes(2) === 120_000);
assertThrow(hours(1) === 3_600_000);
assertThrow(days(1) === 86_400_000);

const timerStart = 100;
const timerEnd = timerStart + seconds(2);
assertThrow(elapsedMs(timerStart, timerEnd) === 2_000);
assertThrow(elapsed(timerStart, "s", timerEnd) === 2);
