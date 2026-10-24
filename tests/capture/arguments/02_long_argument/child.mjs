const [arg = ""] = process.argv.slice(2);
console.log(`${arg.length} characters, all x: ${/^x*$/.test(arg)}`);
