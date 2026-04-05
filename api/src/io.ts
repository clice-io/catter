import * as capi from "catter-c";
import { io } from "./index.js";

/**
 * Prints content to standard output without a trailing newline.
 *
 * @param content - The string content to print.
 * @throws Will throw if the underlying output system fails (rare).
 *
 * @example
 * ```typescript
 * print("Hello");
 * print(" World"); // prints "Hello World" on same line
 * ```
 */
export function print(content: string) {
  capi.stdout_print(content);
}

/**
 * Prints content to standard output with a trailing newline.
 *
 * Equivalent to `print(content + "\n")`.
 *
 * @param content - The string content to print.
 * @throws Will throw if the underlying output system fails (rare).
 *
 * @example
 * ```typescript
 * println("Hello World"); // prints "Hello World\n"
 * println("Next line");   // prints on next line
 * ```
 */
export function println(content: string) {
  capi.stdout_print(content + "\n");
}

/**
 * Prints colored content to standard output without a trailing newline.
 *
 * The color is applied via ANSI escape codes by the underlying output system.
 *
 * @param content - The string content to print.
 * @param color - The color to apply: `"red"`, `"yellow"`, `"blue"`, or `"green"`.
 * @throws Will throw if the underlying output system fails (rare).
 *
 * @example
 * ```typescript
 * coloredPrint("Warning: ", "yellow");
 * coloredPrint("critical error", "red");
 * ```
 */
export function coloredPrint(
  content: string,
  color: "red" | "yellow" | "blue" | "green",
) {
  switch (color) {
    case "red":
      capi.stdout_print_red(content);
      break;
    case "yellow":
      capi.stdout_print_yellow(content);
      break;
    case "blue":
      capi.stdout_print_blue(content);
      break;
    case "green":
      capi.stdout_print_green(content);
      break;
  }
}

/**
 * Prints colored content to standard output with a trailing newline.
 *
 * Equivalent to `coloredPrint(content + "\n", color)`.
 *
 * @param content - The string content to print.
 * @param color - The color to apply: `"red"`, `"yellow"`, `"blue"`, or `"green"`.
 * @throws Will throw if the underlying output system fails (rare).
 *
 * @example
 * ```typescript
 * coloredPrintln("Success!", "green");
 * coloredPrintln("Error occurred", "red");
 * ```
 */
export function coloredPrintln(
  content: string,
  color: "red" | "yellow" | "blue" | "green",
) {
  return coloredPrint(content + "\n", color);
}

/**
 * Returns the path separator for the current platform.
 *
 * @returns `"\\"` on Windows and `"/"` on other platforms.
 *
 * @example
 * ```typescript
 * const cachePath = "tmp" + dir_sep() + "cache.txt";
 * ```
 */
export function dir_sep(): "/" | "\\" {
  return capi.os_name() === "windows" ? "\\" : "/";
}

/**
 * Enumeration for file seek position reference.
 *
 * - `SET` (0): Seek from the beginning of the file.
 * - `CUR` (1): Seek from the current position.
 * - `END` (2): Seek from the end of the file.
 *
 * @example
 * ```typescript
 * stream.seekRead(0, SeekWhence.SET);
 * stream.seekWrite(0, SeekWhence.END);
 * ```
 */
export enum SeekWhence {
  SET = 0,
  CUR = 1,
  END = 2,
}

/**
 * Low-level binary file stream for reading and writing raw bytes.
 *
 * Manages an open file handle in the C layer. The file is opened in read-write mode
 * (`std::ios::in | std::ios::out`). Must be closed explicitly via `close()` or used
 * with the `with()` static method to ensure proper resource cleanup.
 *
 * All operations are single-threaded by design; concurrent access from multiple threads
 * will result in undefined behavior.
 *
 * @example
 * ```typescript
 * const stream = new FileStream("data.bin");
 * const bytes = stream.read(10);
 * stream.close();
 * ```
 *
 * @example
 * ```typescript
 * // Using with() for automatic cleanup
 * FileStream.with("data.bin", (stream) => {
 *   const data = stream.readEntireFile();
 *   println("Read " + data.length + " bytes");
 * });
 * ```
 */
export class FileStream {
  private fd: number;

  /**
   * Opens a file at the specified path in read-write mode.
   *
   * @param path - The file path. Can be relative or absolute.
   * @throws Will throw if the file cannot be opened (does not exist, permission denied, etc.).
   */
  public constructor(path: string) {
    this.fd = capi.file_open(path);
  }

  /**
   * Closes the file handle and releases the file ID.
   *
   * After calling `close()`, all subsequent operations on this stream will throw.
   * Safe to call multiple times on the same stream.
   *
   * @throws Will throw if the file ID is invalid (e.g., already closed).
   *
   * @example
   * ```typescript
   * const stream = new FileStream("file.bin");
   * stream.write(new Uint8Array([1, 2, 3]));
   * stream.close(); // cleanup
   * ```
   */
  public close(): void {
    capi.file_close(this.fd);
  }

  /**
   * Seeks the write position to an offset relative to a reference point.
   *
   * The seek operation clears any error state before seeking.
   *
   * @param offset - The byte offset to seek to (can be negative for `CUR` or `END`).
   * @param whence - The reference point: `SET` (beginning), `CUR` (current), or `END` (end).
   * @throws Will throw if the file ID is invalid or if `whence` is not 0, 1, or 2.
   *
   * @example
   * ```typescript
   * stream.seekWrite(100, SeekWhence.SET);   // seek to byte 100
   * stream.seekWrite(-10, SeekWhence.END);   // seek 10 bytes before EOF
   * stream.seekWrite(0, SeekWhence.END);     // seek to EOF
   * ```
   */
  public seekWrite(offset: number, whence: SeekWhence): void {
    capi.file_seek_write(this.fd, offset, whence);
  }

  /**
   * Seeks the read position to an offset relative to a reference point.
   *
   * The seek operation clears any error state before seeking.
   *
   * @param offset - The byte offset to seek to (can be negative for `CUR` or `END`).
   * @param whence - The reference point: `SET` (beginning), `CUR` (current), or `END` (end).
   * @throws Will throw if the file ID is invalid or if `whence` is not 0, 1, or 2.
   *
   * @example
   * ```typescript
   * stream.seekRead(0, SeekWhence.SET);      // seek to beginning
   * stream.seekRead(50, SeekWhence.CUR);     // seek 50 bytes forward
   * ```
   */
  public seekRead(offset: number, whence: SeekWhence): void {
    capi.file_seek_read(this.fd, offset, whence);
  }

  /**
   * Reads up to `n` bytes from the file at the current read position.
   *
   * Returns a `Uint8Array` containing the bytes actually read, which may be fewer than
   * `n` if EOF is reached. Returns an empty array if `n` is 0.
   *
   * @param n - The maximum number of bytes to read. Must be non-negative.
   * @returns A `Uint8Array` with the bytes read (may have length less than n if EOF reached).
   * @throws Will throw if `n` is negative, if the file ID is invalid, or if
   *         the ArrayBuffer cannot be created or accessed.
   *
   * @example
   * ```typescript
   * const chunk = stream.read(1024);
   * println("Read " + chunk.length + " bytes");
   * ```
   */
  public read(n: number): Uint8Array {
    if (n < 0) {
      throw new TypeError("n must be positive");
    }
    if (n == 0) {
      return new Uint8Array(0);
    }
    const buf = new ArrayBuffer(n);
    const bytesRead = capi.file_read_n(this.fd, n, buf);
    return new Uint8Array(buf, 0, bytesRead);
  }

  /**
   * Reads bytes from the file into an existing `Uint8Array` buffer.
   *
   * Reads up to `into.length` bytes into the provided buffer. The buffer is not resized;
   * it is written to in-place from offset 0.
   *
   * @param into - The `Uint8Array` to read into. The underlying `ArrayBuffer` is accessed.
   * @returns The number of bytes actually read. May be less than `into.length` if EOF reached.
   * @throws Will throw if the file ID is invalid or if the ArrayBuffer cannot be accessed.
   *
   * @example
   * ```typescript
   * const buf = new Uint8Array(256);
   * const bytesRead = stream.readBuf(buf);
   * println("Read " + bytesRead + " bytes into buffer");
   * // Only buf.slice(0, bytesRead) contains valid data
   * ```
   */
  public readBuf(into: Uint8Array): number {
    const bytesRead = capi.file_read_n(
      this.fd,
      into.length,
      into.buffer as ArrayBuffer,
    );
    return bytesRead;
  }

  /**
   * Writes bytes to the file at the current write position.
   *
   * All bytes in the `Uint8Array` are written. The write position advances by the number
   * of bytes written. Behavior if the buffer is modified during the write is undefined.
   *
   * @param data - The `Uint8Array` containing bytes to write.
   * @throws Will throw if the file ID is invalid or if the ArrayBuffer cannot be accessed.
   *
   * @example
   * ```typescript
   * const data = new Uint8Array([1, 2, 3, 4, 5]);
   * stream.write(data);
   * ```
   */
  public write(data: Uint8Array): void {
    capi.file_write_n(this.fd, data.length, data.buffer as ArrayBuffer);
  }

  /**
   * Appends bytes to the end of the file.
   *
   * Seeks to EOF, then writes. Equivalent to `seekWrite(0, SeekWhence.END); write(data)`.
   *
   * @param data - The `Uint8Array` containing bytes to append.
   * @throws Will throw if seek or write fails.
   *
   * @example
   * ```typescript
   * stream.append(new Uint8Array([99, 100])); // add to end of file
   * ```
   */
  public append(data: Uint8Array): void {
    this.seekWrite(0, SeekWhence.END);
    this.write(data);
  }

  /**
   * Gets the total size of the file in bytes.
   *
   * Temporarily seeks to EOF to determine size, then restores the read position.
   * The write position is not restored.
   *
   * @returns The file size in bytes.
   * @throws Will throw if seek or tell operations fail.
   *
   * @example
   * ```typescript
   * const size = stream.file_size();
   * println("File is " + size + " bytes");
   * ```
   */
  public file_size(): number {
    const currentPos = capi.file_tell_read(this.fd);
    this.seekRead(0, SeekWhence.END);
    const size = capi.file_tell_read(this.fd);
    this.seekRead(currentPos, SeekWhence.SET);
    return size;
  }

  /**
   * Gets the current read position in the file.
   *
   * @returns The byte offset of the read pointer from the beginning of the file.
   * @throws Will throw if the file ID is invalid.
   *
   * @example
   * ```typescript
   * stream.read(100);
   * println("Position: " + stream.tellRead()); // 100
   * ```
   */
  public tellRead(): number {
    return capi.file_tell_read(this.fd);
  }

  /**
   * Gets the current write position in the file.
   *
   * @returns The byte offset of the write pointer from the beginning of the file.
   * @throws Will throw if the file ID is invalid.
   *
   * @example
   * ```typescript
   * stream.write(data);
   * println("Position: " + stream.tellWrite());
   * ```
   */
  public tellWrite(): number {
    return capi.file_tell_write(this.fd);
  }

  /**
   * Reads the entire file contents into memory as a `Uint8Array`.
   *
   * Seeks to EOF to determine file size, seeks back to start, then reads all bytes.
   * The read position is left at EOF.
   *
   * @returns A `Uint8Array` containing all file bytes.
   * @throws Will throw if file size, seek, or read operations fail.
   *
   * @example
   * ```typescript
   * const allBytes = stream.readEntireFile();
   * println("File size: " + allBytes.length);
   * ```
   */
  public readEntireFile(): Uint8Array {
    const size = this.file_size();
    this.seekRead(0, SeekWhence.SET);
    return this.read(size);
  }

  /**
   * Opens a file, executes a callback with the stream, and ensures cleanup.
   *
   * Automatically closes the stream after the callback completes or if it throws.
   * This is the recommended way to use FileStream to prevent resource leaks.
   *
   * @param path - The file path to open.
   * @param callback - A function receiving the open FileStream.
   * @throws Will throw if the file cannot be opened or if the callback throws
   *         (after cleanup).
   *
   * @example
   * ```typescript
   * FileStream.with("data.bin", (stream) => {
   *   const data = stream.readEntireFile();
   *   println("Read " + data.length + " bytes");
   * }); // stream auto-closed
   * ```
   */
  static with(path: string, callback: (stream: FileStream) => void) {
    const stream = new FileStream(path);
    try {
      callback(stream);
    } catch (e) {
      stream.close();
      throw e;
    }
    stream.close();
  }
}

/**
 * Type alias for supported text file encodings.
 *
 * Currently `"ascii"` and `"utf-8"` are supported.
 *
 * @example
 * ```typescript
 * const encoding: SupportedTextEncodings = "utf-8";
 * ```
 */
export type SupportedTextEncodings =
  (typeof TextFileStream.supportedEncodings)[number];

/**
 * Interface for encoding/decoding operations on binary streams.
 *
 * Implementations provide symmetrical read/write operations for a specific character encoding.
 *
 * @internal
 */
export interface EnDecStreamImpl {
  /**
   * Reads and decodes a fixed number of characters.
   *
   * @param raw - The underlying binary stream to read from.
   * @param chars - The maximum number of decoded characters to read.
   * @returns The decoded string built from the consumed bytes.
   */
  read(raw: FileStream, chars: number): string;

  /**
   * Encodes and writes a string.
   *
   * @param raw - The underlying binary stream to write into.
   * @param data - The decoded text that should be encoded before writing.
   */
  write(raw: FileStream, data: string): void;

  /**
   * Reads until a delimiter or EOF is reached.
   *
   * @param raw - The underlying binary stream to read from.
   * @param delimiter - The delimiter character to stop at, or `null` to read until EOF.
   * @param bufSize - The temporary byte-buffer size used for chunked reads.
   * @returns The decoded string read before the delimiter or EOF.
   */
  readUntil(raw: FileStream, delimiter: string | null, bufSize: number): string;

  /**
   * Decodes raw bytes to a string.
   *
   * @param raw - The raw bytes that should be decoded into text.
   * @returns The decoded string value.
   */
  decode(raw: Uint8Array): string;

  /**
   * Encodes a string to raw bytes.
   *
   * @param data - The text that should be converted into encoded bytes.
   * @returns The encoded byte sequence.
   */
  encode(data: string): Uint8Array;
}

const asciiEnDecStreamImpl: EnDecStreamImpl = {
  write(raw: FileStream, data: string): void {
    raw.write(this.encode(data));
  },
  readUntil(
    raw: FileStream,
    delimiter: string | null,
    bufSize: number = 32,
  ): string {
    const needCmp = delimiter !== null;
    const delimiterCode = needCmp ? delimiter!.charCodeAt(0) : -1;
    let result = "";
    const buffer = new Uint8Array(bufSize);
    while (true) {
      const bytesRead = raw.readBuf(buffer);
      for (let i = 0; i < bytesRead; i++) {
        const byte = buffer[i];
        if (byte === delimiterCode) {
          raw.seekRead(i - bytesRead + 1, SeekWhence.CUR);
          return result;
        } else {
          result += String.fromCharCode(byte);
        }
      }
      if (bytesRead < bufSize) {
        break;
      }
    }
    return result;
  },
  read(raw: FileStream, chars: number): string {
    const bytes = raw.read(chars);
    return this.decode(bytes);
  },
  decode: function (raw: Uint8Array): string {
    const chunkSize = 0x8000;
    let result = "";
    for (let i = 0; i < raw.length; i += chunkSize) {
      result += String.fromCharCode(...raw.slice(i, i + chunkSize));
    }
    return result;
  },
  encode: function (data: string): Uint8Array {
    return new Uint8Array([...data].map((c) => c.charCodeAt(0)));
  },
};

function utf8SequenceLength(firstByte: number): number {
  if ((firstByte & 0b1000_0000) === 0) {
    return 1;
  }
  if ((firstByte & 0b1110_0000) === 0b1100_0000) {
    return 2;
  }
  if ((firstByte & 0b1111_0000) === 0b1110_0000) {
    return 3;
  }
  if ((firstByte & 0b1111_1000) === 0b1111_0000) {
    return 4;
  }
  throw new Error(`Invalid UTF-8 leading byte: ${firstByte}`);
}

function utf8DecodeCodePoint(bytes: number[]): number {
  switch (bytes.length) {
    case 1:
      return bytes[0];
    case 2:
      return ((bytes[0] & 0b0001_1111) << 6) | (bytes[1] & 0b0011_1111);
    case 3:
      return (
        ((bytes[0] & 0b0000_1111) << 12) |
        ((bytes[1] & 0b0011_1111) << 6) |
        (bytes[2] & 0b0011_1111)
      );
    case 4:
      return (
        ((bytes[0] & 0b0000_0111) << 18) |
        ((bytes[1] & 0b0011_1111) << 12) |
        ((bytes[2] & 0b0011_1111) << 6) |
        (bytes[3] & 0b0011_1111)
      );
    default:
      throw new Error(`Unsupported UTF-8 sequence length: ${bytes.length}`);
  }
}

function readUtf8Sequence(raw: FileStream): number[] | undefined {
  const first = raw.read(1);
  if (first.length === 0) {
    return undefined;
  }

  const bytes = [first[0]];
  const expectedLength = utf8SequenceLength(first[0]);
  if (expectedLength === 1) {
    return bytes;
  }

  const tail = raw.read(expectedLength - 1);
  if (tail.length !== expectedLength - 1) {
    throw new Error("Truncated UTF-8 sequence");
  }
  for (const byte of tail) {
    bytes.push(byte);
  }
  return bytes;
}

const utf8EnDecStreamImpl: EnDecStreamImpl = {
  write(raw: FileStream, data: string): void {
    raw.write(this.encode(data));
  },
  readUntil(raw: FileStream, delimiter: string | null): string {
    const delimiterBytes = delimiter === null ? null : this.encode(delimiter);
    const collected: number[] = [];
    const matchedDelimiter: number[] = [];

    while (true) {
      const next = raw.read(1);
      if (next.length === 0) {
        collected.push(...matchedDelimiter);
        break;
      }

      if (delimiterBytes === null) {
        collected.push(next[0]);
        continue;
      }

      matchedDelimiter.push(next[0]);
      const matchedPrefix = delimiterBytes
        .slice(0, matchedDelimiter.length)
        .every((byte, index) => byte === matchedDelimiter[index]);
      if (matchedPrefix) {
        if (matchedDelimiter.length === delimiterBytes.length) {
          break;
        }
        continue;
      }

      collected.push(...matchedDelimiter);
      matchedDelimiter.length = 0;
    }

    return this.decode(new Uint8Array(collected));
  },
  read(raw: FileStream, chars: number): string {
    let result = "";
    for (let count = 0; count < chars; ++count) {
      const bytes = readUtf8Sequence(raw);
      if (bytes === undefined) {
        break;
      }
      result += String.fromCodePoint(utf8DecodeCodePoint(bytes));
    }
    return result;
  },
  decode(raw: Uint8Array): string {
    let result = "";
    for (let index = 0; index < raw.length; ) {
      const expectedLength = utf8SequenceLength(raw[index]);
      if (index + expectedLength > raw.length) {
        throw new Error("Truncated UTF-8 sequence");
      }
      const bytes = Array.from(raw.slice(index, index + expectedLength));
      result += String.fromCodePoint(utf8DecodeCodePoint(bytes));
      index += expectedLength;
    }
    return result;
  },
  encode(data: string): Uint8Array {
    const bytes: number[] = [];
    for (const char of data) {
      const codePoint = char.codePointAt(0);
      if (codePoint === undefined) {
        continue;
      }
      if (codePoint <= 0x7f) {
        bytes.push(codePoint);
      } else if (codePoint <= 0x7ff) {
        bytes.push(
          0b1100_0000 | (codePoint >> 6),
          0b1000_0000 | (codePoint & 0b0011_1111),
        );
      } else if (codePoint <= 0xffff) {
        bytes.push(
          0b1110_0000 | (codePoint >> 12),
          0b1000_0000 | ((codePoint >> 6) & 0b0011_1111),
          0b1000_0000 | (codePoint & 0b0011_1111),
        );
      } else {
        bytes.push(
          0b1111_0000 | (codePoint >> 18),
          0b1000_0000 | ((codePoint >> 12) & 0b0011_1111),
          0b1000_0000 | ((codePoint >> 6) & 0b0011_1111),
          0b1000_0000 | (codePoint & 0b0011_1111),
        );
      }
    }
    return new Uint8Array(bytes);
  },
};

/**
 * High-level text file stream for reading and writing encoded text.
 *
 * Wraps a FileStream with encoding/decoding support. Currently ASCII and UTF-8
 * encoding is supported. Handles line-based operations like readLine() and readLines().
 *
 * Must be closed explicitly via close() or used with the with() static method
 * to ensure proper resource cleanup.
 *
 * @example
 * ```typescript
 * const stream = new TextFileStream("text.txt", "utf-8");
 * const line = stream.readLine();
 * stream.close();
 * ```
 *
 * @example
 * ```typescript
 * // Using with() for automatic cleanup
 * TextFileStream.with("text.txt", "utf-8", (stream) => {
 *   const lines = stream.readLines();
 *   println("Read " + lines.length + " lines");
 * });
 * ```
 */
export class TextFileStream {
  /**
   * Array of encoding names supported by this implementation.
   *
   * Currently `"ascii"` and `"utf-8"` are supported.
   */
  static supportedEncodings = ["ascii", "utf-8"] as const;

  /**
   * Map of encoding implementations keyed by encoding name.
   */
  private static encodingImpls: {
    [key in SupportedTextEncodings]: EnDecStreamImpl;
  } = {
    ascii: asciiEnDecStreamImpl,
    "utf-8": utf8EnDecStreamImpl,
  };

  private fileStream: FileStream;
  private encodingImpl: EnDecStreamImpl;

  /**
   * Opens a text file with the specified encoding.
   *
   * @param path - The file path. Can be relative or absolute.
   * @param encoding - The character encoding to use. Defaults to `"ascii"`.
   *                   Supported values are `"ascii"` and `"utf-8"`.
   * @throws Will throw if the file cannot be opened or if the encoding is not supported.
   */
  constructor(path: string, encoding: SupportedTextEncodings = "ascii") {
    this.fileStream = new FileStream(path);
    if (!TextFileStream.supportedEncodings.includes(encoding)) {
      throw new TypeError("Unsupported encoding: " + encoding);
    }
    this.encodingImpl = TextFileStream.encodingImpls[encoding];
  }

  /**
   * Closes the underlying file stream.
   *
   * After calling close(), all subsequent operations will throw.
   *
   * @throws Will throw if the file stream is invalid.
   */
  public close(): void {
    this.fileStream.close();
  }

  /**
   * Writes an encoded string to the file at the current write position.
   *
   * @param data - The string to write.
   * @throws Will throw if the underlying write fails.
   *
   * @example
   * ```typescript
   * stream.write("Hello, World!");
   * ```
   */
  public write(data: string): void {
    this.encodingImpl.write(this.fileStream, data);
  }

  /**
   * Reads and decodes a fixed number of characters.
   *
   * May read fewer characters if EOF is reached.
   *
   * @param chars - The number of characters to read.
   * @returns The decoded string (may be shorter than `chars` if EOF reached).
   * @throws Will throw if the underlying read fails.
   *
   * @example
   * ```typescript
   * const text = stream.read(50);
   * ```
   */
  public read(chars: number): string {
    return this.encodingImpl.read(this.fileStream, chars);
  }

  /**
   * Reads until a delimiter character is encountered or EOF is reached.
   *
   * The delimiter is consumed from the stream but NOT included in the returned string.
   * Reads in buffered chunks (default 32 bytes) for efficiency.
   *
   * @param delimiter - The character that marks the end of the read. Common examples: `"\n"` for lines.
   * @returns The string up to (but not including) the delimiter. Returns everything up to EOF
   *          if the delimiter is not found.
   * @throws Will throw if the underlying read fails.
   *
   * @example
   * ```typescript
   * const line = stream.readUntil("\n"); // read until newline
   * ```
   */
  public readUntil(delimiter: string): string {
    return this.encodingImpl.readUntil(this.fileStream, delimiter, 32);
  }

  /**
   * Appends an encoded string to the end of the file.
   *
   * Seeks to EOF before writing.
   *
   * @param data - The string to append.
   * @throws Will throw if seek or write fails.
   *
   * @example
   * ```typescript
   * stream.append("\nNew line at end");
   * ```
   */
  public append(data: string): void {
    this.fileStream.seekWrite(0, SeekWhence.END);
    this.write(data);
  }

  /**
   * Reads a single line (text up to and including the newline character).
   *
   * The newline character (`"\n"`) is consumed but NOT included in the returned string.
   *
   * @returns The line as a string without the trailing newline.
   * @throws Will throw if the underlying read fails.
   *
   * @example
   * ```typescript
   * const firstLine = stream.readLine();
   * ```
   */
  public readLine(): string {
    let res = this.readUntil("\n");
    if (res.endsWith("\r")) {
      res = res.slice(0, -1);
    }
    return res;
  }

  /**
   * Reads the entire file contents as a single decoded string.
   *
   * Determines file size, seeks to start, reads all bytes, and decodes them.
   *
   * @returns The complete file contents as a string.
   * @throws Will throw if file size, seek, or read operations fail.
   *
   * @example
   * ```typescript
   * const content = stream.readEntireFile();
   * println(content);
   * ```
   */
  public readEntireFile(): string {
    const raw = this.fileStream.readEntireFile();
    return this.encodingImpl.decode(raw);
  }

  /**
   * Reads all lines from the file into an array of strings.
   *
   * Repeatedly calls readLine() until EOF is reached. Empty strings indicate
   * consecutive newlines; the loop terminates when an empty line is encountered at EOF.
   *
   * Note: This implementation treats empty lines specially. A file ending with
   * a newline will have an empty string as the last readLine() result, which terminates
   * the loop. Files without a trailing newline are read completely.
   *
   * @returns An array of lines, each without its trailing newline.
   * @throws Will throw if the underlying read fails.
   *
   * @example
   * ```typescript
   * const lines = stream.readLines();
   * println("Total lines: " + lines.length);
   * ```
   */
  public readLines(): string[] {
    const lines: string[] = [];
    let currentPos = this.fileStream.tellRead();
    while (true) {
      const line = this.readLine();
      const newPos = this.fileStream.tellRead();
      if (line === "" && newPos === currentPos) {
        // EOF reached with empty line
        break;
      }
      lines.push(line);
      currentPos = newPos;
    }
    return lines;
  }

  /**
   * Opens a text file, executes a callback with the stream, and ensures cleanup.
   *
   * Automatically closes the stream after the callback completes or if it throws.
   * This is the recommended way to use TextFileStream to prevent resource leaks.
   *
   * @param path - The file path to open.
   * @param encoding - The character encoding to use. Supported values are `"ascii"` and `"utf-8"`.
   * @param callback - A function receiving the open TextFileStream.
   * @throws Will throw if the file cannot be opened, if the encoding is unsupported,
   *         or if the callback throws (after cleanup).
   *
   * @example
   * ```typescript
   * TextFileStream.with("log.txt", "utf-8", (stream) => {
   *   const lines = stream.readLines();
   *   for (let i = 0; i < lines.length; i++) {
   *     println(lines[i]);
   *   }
   * }); // stream auto-closed
   * ```
   */
  static with(
    path: string,
    encoding: SupportedTextEncodings,
    callback: (stream: TextFileStream) => void,
  ) {
    const stream = new TextFileStream(path, encoding);
    try {
      callback(stream);
    } catch (e) {
      stream.close();
      throw e;
    }
    stream.close();
  }
}
