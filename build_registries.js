const fs = require("fs/promises");
const path = require("path");

// Extract item and block data from registry dump
async function extractItemsAndBlocks () {

  // Block network IDs are defined in their own JSON file
  const blockSource = JSON.parse(await fs.readFile(`${__dirname}/notchian/generated/reports/blocks.json`, "utf8"));
  // Items don't seem to have network IDs outside of registries.json
  const itemSource = JSON.parse(await fs.readFile(`${__dirname}/notchian/generated/reports/registries.json`, "utf8"))["minecraft:item"].entries;

  // Sort blocks by their network ID
  // Since we're only storing 256 blocks, this prioritizes the "common" ones first
  const sortedBlocks = Object.entries(blockSource);
  sortedBlocks.sort((a, b) => {
    const aState = a[1].states.find(c => c.default);
    if (!aState) return 1;
    const bState = b[1].states.find(c => c.default);
    if (!bState) return -1;
    return aState.id - bState.id;
  });

  // Create name-id pair objects for easier parsing
  const blocks = {}, items = {};

  for (const entry of sortedBlocks) {
    const defaultState = entry[1].states.find(c => c.default);
    if (!defaultState) continue;
    blocks[entry[0].replace("minecraft:", "")] = defaultState.id;
  }

  for (const item in itemSource) {
    items[item.replace("minecraft:", "")] = itemSource[item].protocol_id;
  }

  /**
   * Create a block network ID palette.
   * The goal is to pack as many meaningful blocks into 256 IDs as
   * possible. For this, we only include blocks that have corresponding
   * items, outside of some exceptions.
   */
  const palette = {};
  const exceptions = [ "air", "water", "lava" ];

  // While we're at it, map block IDs to item IDs
  const mapping = [];

  for (const block in blocks) {
    if (!(block in items) && !exceptions.includes(block)) continue;
    palette[block] = blocks[block];
    mapping.push(items[block] || 0);
    if (mapping.length === 256) break;
  }

  return { blocks, items, palette, mapping };

}

// Write an integer as a VarInt
function writeVarInt (value) {
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

// Scan directory recursively to find all JSON files
async function scanDirectory (basePath, currentPath = "") {
  const entries = {};
  const items = await fs.readdir(path.join(basePath, currentPath), { withFileTypes: true });

  for (const item of items) {
    const relativePath = path.join(currentPath, item.name);
    if (item.isDirectory()) {
      const subEntries = await scanDirectory(basePath, relativePath);
      Object.assign(entries, subEntries);
    } else if (item.name.endsWith(".json")) {
      const dirPath = path.dirname(relativePath);
      const registryName = dirPath === "." ? "" : dirPath;
      const entryName = path.basename(item.name, ".json");

      if (!entries[registryName]) {
        entries[registryName] = [];
      }
      entries[registryName].push(entryName);
    }
  }

  return entries;
}

// Serialize a single registry
function serializeRegistry (name, entries) {
  const parts = [];

  // Packet ID for Registry Data
  parts.push(Buffer.from([0x07]));

  // Registry name
  const nameBuf = Buffer.from(name, "utf8");
  parts.push(writeVarInt(nameBuf.length));
  parts.push(nameBuf);

  // Entry count
  parts.push(writeVarInt(entries.length));

  // Serialize entries
  for (const entryName of entries) {
    const entryBuf = Buffer.from(entryName, "utf8");
    parts.push(writeVarInt(entryBuf.length));
    parts.push(entryBuf);
    parts.push(Buffer.from([0x00]));
  }

  // Combine all parts
  const fullData = Buffer.concat(parts);

  // Prepend packet length
  const lengthBuf = writeVarInt(fullData.length);

  return Buffer.concat([lengthBuf, fullData]);
}

// Convert to C-style hex byte array string
function toCArray (buffer) {
  const hexBytes = [...buffer].map(b => `0x${b.toString(16).padStart(2, "0")}`);
  const lines = [];
  for (let i = 0; i < hexBytes.length; i += 12) {
    lines.push("  " + hexBytes.slice(i, i + 12).join(", "));
  }
  return lines.join(",\n");
}

const requiredRegistries = [
  "dimension_type",
  "cat_variant",
  "chicken_variant",
  "cow_variant",
  "frog_variant",
  "painting_variant",
  "pig_variant",
  "wolf_sound_variant",
  "wolf_variant",
  "damage_type",
  "worldgen/biome"
];

async function convert () {

  const inputPath = __dirname + "/notchian/generated/data/minecraft";
  const outputPath = __dirname + "/src/registries.c";
  const headerPath = __dirname + "/src/registries.h";

  const registries = await scanDirectory(inputPath);
  const buffers = [];

  for (const registry of requiredRegistries) {
    if (!(registry in registries)) {
      console.error(`Missing required registry "${registry}"!`);
      return;
    }
    buffers.push(serializeRegistry(registry, registries[registry]));
  }

  const itemsAndBlocks = await extractItemsAndBlocks();

  const output = Buffer.concat(buffers);
  const cArray = toCArray(output);
  const sourceCode = `\
#include <stdint.h>
#include "registries.h"

// Binary contents of required "Registry Data" packets
uint8_t registries_bin[] = {
${cArray}
};

// Block palette
uint16_t block_palette[] = { ${Object.values(itemsAndBlocks.palette).join(", ")} };
// Block-to-item mapping
uint16_t B_to_I[] = { ${itemsAndBlocks.mapping.join(", ")} };
// Item-to-block mapping
uint8_t I_to_B (uint16_t item) {
  switch (item) {
    ${itemsAndBlocks.mapping.map((c, i) => c ? `case ${c}: return ${i};\n    ` : "").join("")}
    default: break;
  }
  return 0;
}
`;

  const headerCode = `\
#ifndef H_REGISTRIES
#define H_REGISTRIES

#include <stdint.h>

extern uint8_t registries_bin[${output.length}];

extern uint16_t block_palette[256]; // Block palette
extern uint16_t B_to_I[256]; // Block-to-item mapping
uint8_t I_to_B (uint16_t item); // Item-to-block mapping

// Block identifiers
${Object.keys(itemsAndBlocks.palette).map((c, i) => `#define B_${c} ${i}`).join("\n")}

// Item identifiers
${Object.entries(itemsAndBlocks.items).map(c => `#define I_${c[0]} ${c[1]}`).join("\n")}

// Biome identifiers
${registries["worldgen/biome"].map((c, i) => `#define W_${c} ${i}`).join("\n")}

#endif
`;

  await fs.writeFile(outputPath, sourceCode);
  await fs.writeFile(headerPath, headerCode);
  console.log("Done. Wrote to `registries.c` and `registries.h`");

}

convert().catch(console.error);
