#!/usr/bin/env node
/**
 * Quick Start Example - Basic compression/decompression
 * Run: node quick-example.js
 */

const { join } = require('path');
const addon = require(join(__dirname, 'sevenzip-napi.node'));

console.log('🗜️  7z Compression Quick Start\n');
console.log(`Using LZMA SDK ${addon.lzmaVersion()}\n`);

// Example 1: Simple Text Compression
async function example1_simpleText() {
  console.log('📝 Example 1: Compress Text');
  console.log('─────────────────────────────');
  
  const text = 'Hello, World! This is a test of 7z compression. '.repeat(10);
  const input = Buffer.from(text);
  
  console.log(`Input: ${input.length} bytes`);
  
  // Compress with level 5 (balanced speed/ratio)
  const compressed = await addon.compress(input, { level: 5 });
  console.log(`Compressed: ${compressed.length} bytes`);
  console.log(`Ratio: ${((1 - compressed.length / input.length) * 100).toFixed(1)}% smaller\n`);
  
  // Decompress
  const decompressed = await addon.decompress(compressed);
  console.log(`Decompressed: ${decompressed.length} bytes`);
  console.log(`✓ Match: ${input.equals(decompressed)}\n`);
}

// Example 2: Different Compression Levels
async function example2_compressionLevels() {
  console.log('⚡ Example 2: Compression Levels');
  console.log('─────────────────────────────────');
  
  const data = Buffer.from('The quick brown fox jumps over the lazy dog. '.repeat(50));
  
  for (const level of [0, 3, 5, 9]) {
    const start = Date.now();
    const compressed = await addon.compress(data, { level });
    const time = Date.now() - start;
    
    const ratio = ((1 - compressed.length / data.length) * 100).toFixed(1);
    console.log(`Level ${level}: ${compressed.length} bytes (${ratio}% reduction) - ${time}ms`);
  }
  console.log();
}

// Example 3: XZ Format
async function example3_xzFormat() {
  console.log('🔧 Example 3: XZ Format');
  console.log('────────────────────────');
  
  const text = 'XZ format test with LZMA2 compression';
  const input = Buffer.from(text);
  
  // Compress with XZ format
  const compressed = await addon.compress(input, { level: 6, useXz: true });
  console.log(`Input: ${input.length} bytes`);
  console.log(`XZ compressed: ${compressed.length} bytes`);
  
  // Decompress (auto-detects XZ format)
  const decompressed = await addon.decompress(compressed);
  console.log(`Decompressed: "${decompressed.toString()}"`);
  console.log(`✓ Match: ${input.equals(decompressed)}\n`);
}

// Example 4: Pre-allocate Buffer
async function example4_preAllocate() {
  console.log('💾 Example 4: Buffer Pre-allocation');
  console.log('─────────────────────────────────────');
  
  const input = Buffer.alloc(10240, 0xFF); // 10 KB of data
  
  // Calculate maximum possible compressed size
  const maxSize = addon.compressBound(input.length);
  console.log(`Input: ${input.length} bytes`);
  console.log(`Max compressed size: ${maxSize} bytes (${(maxSize / input.length).toFixed(2)}x overhead)`);
  
  const compressed = await addon.compress(input, { level: 5 });
  console.log(`Actual compressed: ${compressed.length} bytes (${(compressed.length / maxSize * 100).toFixed(1)}% of max)\n`);
}

// Example 5: Error Handling
async function example5_errorHandling() {
  console.log('🛡️  Example 5: Error Handling');
  console.log('─────────────────────────────');
  
  try {
    // Try to decompress invalid data
    const badData = Buffer.from('not compressed data');
    await addon.decompress(badData);
  } catch (error) {
    console.log(`✓ Caught error: ${error.message}\n`);
  }
}

// Run all examples
async function main() {
  try {
    await example1_simpleText();
    await example2_compressionLevels();
    await example3_xzFormat();
    await example4_preAllocate();
    await example5_errorHandling();
    
    console.log('✅ All examples completed successfully!');
    console.log('\nNext steps:');
    console.log('  • See napi/QUICKSTART.md for more examples');
    console.log('  • Check napi/examples/node-example.js for advanced usage');
    console.log('  • Read napi/index.d.ts for TypeScript definitions');
  } catch (error) {
    console.error('❌ Error:', error);
    process.exit(1);
  }
}

main();
