import { debug, time } from "catter";

const before = Date.now();
const now = time.now();
const after = Date.now();

debug.assertThrow(now >= before - 1_000);
debug.assertThrow(now <= after + 1_000);
debug.assertThrow(time.unixMs() >= before - 1_000);
debug.assertThrow(time.unixUs() >= (before - 1_000) * 1_000);
debug.assertThrow(Math.abs(time.unixSeconds() * 1_000 - now) < 2_000);

const monotonicA = time.monotonicMs();
const monotonicB = time.monotonicMs();
debug.assertThrow(monotonicB >= monotonicA);

const monotonicUsA = time.monotonicUs();
const monotonicUsB = time.monotonicUs();
debug.assertThrow(monotonicUsB >= monotonicUsA);

debug.assertThrow(time.convert(1, "s", "ms") === 1_000);
debug.assertThrow(time.convert(2, "min", "s") === 120);
debug.assertThrow(time.convert(1, "h", "min") === 60);
debug.assertThrow(time.toMs(1, "d") === 86_400_000);
debug.assertThrow(time.fromMs(1_500, "s") === 1.5);

debug.assertThrow(time.ns(1_000_000) === 1);
debug.assertThrow(time.us(1_000) === 1);
debug.assertThrow(time.ms(9) === 9);
debug.assertThrow(time.seconds(2) === 2_000);
debug.assertThrow(time.minutes(2) === 120_000);
debug.assertThrow(time.hours(1) === 3_600_000);
debug.assertThrow(time.days(1) === 86_400_000);

const timerStart = 100;
const timerEnd = timerStart + time.seconds(2);
debug.assertThrow(time.elapsedMs(timerStart, timerEnd) === 2_000);
debug.assertThrow(time.elapsed(timerStart, "s", timerEnd) === 2);
