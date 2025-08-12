const fs = require("fs");
const path = require("path");

// Write an integer as a varint
function writeVarInt(value) {
  const bytes = [];
  while (true) {
    if ((value & ~0x7F) === 0) {
      bytes.push(value);
      return Buffer.from(bytes);
    }
    bytes.push((value & 0x7F) | 0x80);
    value >>>= 7;
  }
}

// Serialize a single registry
function serializeRegistry(name, entries) {
  const parts = [];

  // Write 0x07
  parts.push(Buffer.from([0x07]));

  // Registry name
  const nameBuf = Buffer.from(name.replace("minecraft:", ""), "utf8");
  parts.push(writeVarInt(nameBuf.length));
  parts.push(nameBuf);

  // Entry count
  const entryKeys = Object.keys(entries);
  parts.push(writeVarInt(entryKeys.length));

  // Serialize entries
  for (const entryName of entryKeys) {
    const entryBuf = Buffer.from(entryName.replace("minecraft:", ""), "utf8");
    parts.push(writeVarInt(entryBuf.length));
    parts.push(entryBuf);
    parts.push(Buffer.from([0x00]));
  }

  // Combine all parts
  const fullData = Buffer.concat(parts);

  // Prepend the length of this registry block as a varint
  const lengthBuf = writeVarInt(fullData.length);

  return Buffer.concat([lengthBuf, fullData]);
}

// Convert to C-style hex byte array string
function toCArray(buffer) {
  const hexBytes = [...buffer].map(b => `0x${b.toString(16).padStart(2, "0")}`);
  const lines = [];
  for (let i = 0; i < hexBytes.length; i += 12) {
    lines.push("  " + hexBytes.slice(i, i + 12).join(", "));
  }
  return lines.join(",\n");
}

// Main function
function convert() {
  const inputPath = path.join(__dirname, "registries.json");
  const outputPath = path.join(__dirname, "_registries.c");
  const headerPath = path.join(__dirname, "_registries.h");

  const json = JSON.parse(fs.readFileSync(inputPath, "utf8"));

  const buffers = [];

  for (const [registryName, entries] of Object.entries(json)) {
    buffers.push(serializeRegistry(registryName, entries));
  }

  const output = Buffer.concat(buffers);
  const cArray = toCArray(output);
  const finalCode = `\
#include <stdint.h>
#include "registries.h"

uint8_t registries_bin[] = {
${cArray}
};
`;

  const headerCode = `\
#ifndef H_REGISTRIES
#define H_REGISTRIES

#include <stdint.h>

extern uint8_t registries_bin[${output.length}];

#endif
`;

  fs.writeFileSync(outputPath, finalCode);
  fs.writeFileSync(headerPath, headerCode);
  console.log("Done. Wrote to `_registries.c` and `_registries.h`");
}

convert();
