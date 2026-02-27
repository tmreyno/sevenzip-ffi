/**
 * Node.js Example - Using @sevenzip/napi
 * 
 * This demonstrates basic usage of the 7z compression library
 * in a Node.js environment.
 */

const { 
  compress, 
  decompress, 
  compressFile, 
  extractFile, 
  lzmaVersion,
  compressBound 
} = require('../sevenzip-napi.node');

async function bufferExample() {
  console.log('=== Buffer Compression Example ===');
  
  // Original data
  const originalText = 'Hello from Node.js! '.repeat(100);
  const input = Buffer.from(originalText);
  
  console.log(`Original size: ${input.length} bytes`);
  
  // Compress
  const compressed = await compress(input, { level: 9 });
  console.log(`Compressed size: ${compressed.length} bytes`);
  console.log(`Compression ratio: ${((1 - compressed.length / input.length) * 100).toFixed(2)}%`);
  
  // Decompress
  const decompressed = await decompress(compressed);
  console.log(`Decompressed size: ${decompressed.length} bytes`);
  console.log(`Match: ${decompressed.toString() === originalText ? '✓' : '✗'}`);
  console.log();
}

async function xzFormatExample() {
  console.log('=== XZ Format Example ===');
  
  const data = Buffer.from('XZ format test data');
  
  // Compress with XZ format
  const compressed = await compress(data, { level: 6, useXz: true });
  console.log(`XZ compressed: ${compressed.length} bytes`);
  
  // Decompress (auto-detects XZ format)
  const decompressed = await decompress(compressed);
  console.log(`Decompressed: ${decompressed.toString()}`);
  console.log();
}

async function fileExample() {
  console.log('=== File Compression Example ===');
  const fs = require('fs').promises;
  
  try {
    // Create test file
    await fs.writeFile('test.txt', 'This is a test file for compression.');
    
    // Compress file to archive
    await compressFile('test.txt', 'output.7z');
    console.log('✓ File compressed to output.7z');
    
    // Extract archive
    await extractFile('output.7z', 'extracted/');
    console.log('✓ Archive extracted to extracted/');
    
    // Verify
    const extracted = await fs.readFile('extracted/test.txt', 'utf-8');
    console.log(`Extracted content: "${extracted}"`);
    
    // Cleanup
    await fs.unlink('test.txt');
    await fs.unlink('output.7z');
    await fs.rm('extracted', { recursive: true, force: true });
    console.log('✓ Cleanup complete');
  } catch (error) {
    console.error('File operation failed:', error.message);
  }
  console.log();
}

async function streamExample() {
  console.log('=== Stream Processing Example ===');
  const { Readable, pipeline } = require('stream');
  const { promisify } = require('util');
  const pipe = promisify(pipeline);
  
  // Create a large data stream
  const dataSize = 1024 * 1024; // 1 MB
  const chunks = [];
  
  const readable = new Readable({
    read() {
      if (chunks.length < 100) {
        this.push(Buffer.alloc(dataSize / 100, 0xFF));
      } else {
        this.push(null);
      }
    }
  });
  
  // Collect chunks
  const allData = [];
  readable.on('data', chunk => allData.push(chunk));
  
  await new Promise(resolve => readable.on('end', resolve));
  
  const combined = Buffer.concat(allData);
  console.log(`Generated ${combined.length} bytes`);
  
  // Calculate required buffer size
  const maxSize = compressBound(combined.length);
  console.log(`Max compressed size: ${maxSize} bytes (${(maxSize / combined.length).toFixed(2)}x)`);
  
  // Compress
  const compressed = await compress(combined, { level: 1 });
  console.log(`Actual compressed: ${compressed.length} bytes (${(compressed.length / combined.length * 100).toFixed(2)}%)`);
  console.log();
}

async function performanceTest() {
  console.log('=== Performance Test ===');
  
  const sizes = [1024, 10240, 102400, 1024000]; // 1KB, 10KB, 100KB, 1MB
  
  for (const size of sizes) {
    const data = Buffer.alloc(size, 0xAB);
    
    const start = Date.now();
    const compressed = await compress(data, { level: 5 });
    const compressTime = Date.now() - start;
    
    const decompressStart = Date.now();
    await decompress(compressed);
    const decompressTime = Date.now() - decompressStart;
    
    console.log(`${(size / 1024).toFixed(0)}KB: compress=${compressTime}ms, decompress=${decompressTime}ms, ratio=${((1 - compressed.length / size) * 100).toFixed(1)}%`);
  }
  console.log();
}

async function main() {
  console.log(`7z Compression Library - LZMA SDK ${lzmaVersion()}`);
  console.log('='.repeat(50));
  console.log();
  
  try {
    await bufferExample();
    await xzFormatExample();
    await fileExample();
    await streamExample();
    await performanceTest();
    
    console.log('✓ All examples completed successfully');
  } catch (error) {
    console.error('Error:', error);
    process.exit(1);
  }
}

if (require.main === module) {
  main().catch(console.error);
}

module.exports = {
  bufferExample,
  xzFormatExample,
  fileExample,
  streamExample,
  performanceTest
};
