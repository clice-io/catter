for (const name of process.argv.slice(2)) {
  console.log(`${name}=${JSON.stringify(process.env[name] ?? null)}`);
}
