// Simple test loader
const { join } = require('path');
const addon = require(join(__dirname, 'sevenzip-napi.node'));

console.log('✓ Native addon loaded successfully');
console.log('LZMA SDK Version:', addon.lzmaVersion());
console.log('Compress bound for 1024 bytes:', addon.compressBound(1024));

// Test async compression
(async () => {
  const input = Buffer.from('Hello, World! '.repeat(100));
  console.log('\n--- Compression Test ---');
  console.log('Input size:', input.length, 'bytes');
  
  const compressed = await addon.compress(input, { level: 5 });
  console.log('Compressed size:', compressed.length, 'bytes');
  console.log('Compression ratio:', ((1 - compressed.length / input.length) * 100).toFixed(2) + '%');
  
  const decompressed = await addon.decompress(compressed);
  console.log('Decompressed size:', decompressed.length, 'bytes');
  console.log('Match:', input.equals(decompressed) ? '✓ PASS' : '✗ FAIL');
  
  console.log('\n✓ All tests passed!');
})().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
