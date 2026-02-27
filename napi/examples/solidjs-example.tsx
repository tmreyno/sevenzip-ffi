/**
 * SolidJS Example - Using @sevenzip/napi
 * 
 * This demonstrates how to use the 7z compression library
 * in a SolidJS application.
 */

import { createSignal, createResource, Show } from 'solid-js';
import { compress, decompress, lzmaVersion } from '@sevenzip/napi';

function App() {
  const [inputText, setInputText] = createSignal('Hello from SolidJS!');
  const [level, setLevel] = createSignal(5);
  const [compressed, setCompressed] = createSignal<Buffer | null>(null);
  const [decompressed, setDecompressed] = createSignal('');
  const [stats, setStats] = createSignal({ original: 0, compressed: 0, ratio: 0 });
  
  // Compress text
  const handleCompress = async () => {
    try {
      const input = Buffer.from(inputText());
      const result = await compress(input, { level: level() });
      
      setCompressed(result);
      setStats({
        original: input.length,
        compressed: result.length,
        ratio: ((1 - result.length / input.length) * 100).toFixed(2)
      });
    } catch (error) {
      console.error('Compression failed:', error);
    }
  };
  
  // Decompress data
  const handleDecompress = async () => {
    try {
      const comp = compressed();
      if (!comp) return;
      
      const result = await decompress(comp);
      setDecompressed(result.toString('utf-8'));
    } catch (error) {
      console.error('Decompression failed:', error);
    }
  };
  
  // File compression example
  const handleFileCompress = async (file: File) => {
    try {
      const arrayBuffer = await file.arrayBuffer();
      const buffer = Buffer.from(arrayBuffer);
      const result = await compress(buffer, { level: 9 });
      
      // Download compressed file
      const blob = new Blob([result], { type: 'application/x-7z-compressed' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${file.name}.7z`;
      a.click();
      URL.revokeObjectURL(url);
    } catch (error) {
      console.error('File compression failed:', error);
    }
  };
  
  return (
    <div class="app">
      <h1>7z Compression with SolidJS</h1>
      <p>Using LZMA SDK {lzmaVersion()}</p>
      
      {/* Compression Demo */}
      <section>
        <h2>Text Compression</h2>
        <textarea
          value={inputText()}
          onInput={(e) => setInputText(e.currentTarget.value)}
          placeholder="Enter text to compress"
          rows={5}
        />
        
        <div class="controls">
          <label>
            Compression Level: {level()}
            <input
              type="range"
              min="0"
              max="9"
              value={level()}
              onInput={(e) => setLevel(parseInt(e.currentTarget.value))}
            />
          </label>
          
          <button onClick={handleCompress}>Compress</button>
        </div>
        
        <Show when={compressed()}>
          <div class="stats">
            <p>Original: {stats().original} bytes</p>
            <p>Compressed: {stats().compressed} bytes</p>
            <p>Ratio: {stats().ratio}% reduction</p>
            
            <button onClick={handleDecompress}>Decompress</button>
          </div>
        </Show>
        
        <Show when={decompressed()}>
          <div class="result">
            <h3>Decompressed:</h3>
            <pre>{decompressed()}</pre>
          </div>
        </Show>
      </section>
      
      {/* File Compression */}
      <section>
        <h2>File Compression</h2>
        <input
          type="file"
          onChange={(e) => {
            const file = e.currentTarget.files?.[0];
            if (file) handleFileCompress(file);
          }}
        />
      </section>
    </div>
  );
}

export default App;
