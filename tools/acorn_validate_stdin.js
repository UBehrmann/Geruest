#!/usr/bin/env node
/**
 * Reads JavaScript from stdin and parses with acorn (npm: acorn).
 * Usage: node tools/acorn_validate_stdin.js < file.js
 * Exit 0 on success, 1 on parse error or missing acorn.
 */
let d = "";
process.stdin.on("data", (c) => (d += c));
process.stdin.on("end", () => {
  try {
    require("acorn").parse(d, {
      ecmaVersion: 2022,
      allowReturnOutsideFunction: true,
    });
    process.exit(0);
  } catch (e) {
    process.stderr.write(String(e) + "\n");
    process.exit(1);
  }
});
