const value = process.env.CATTER_PROBE_BIG ?? "";
console.log(`${value.length} characters, all v: ${/^v*$/.test(value)}`);
