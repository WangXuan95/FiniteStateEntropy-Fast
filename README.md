# FiniteStateEntropy-Fast

我对 Cyan4973 大佬的 FiniteStateEntropy (FSE) 熵编码器使用 AVX2 指令集进行改造，**将FSE解压带宽从 586MB/s 优化到 1956MB/s** （在 Intel Core i7 12700H上，单核）。

需要注意的是，优化后的 FSE 压缩格式和优化前不兼容，因为我调整了压缩流的组织格式，使它更方便用 AVX2 进行优化。



## 核心改动代码

- 对于压缩，详见 `./lib/fse_compress.c` 的 `FSE_compress_stream8_interleave2` 函数
- 对于解压，详见 `/lib/fse_decompress.c` 的 `FSE_decompress_stream8_interleave2_AVX2` 函数



## 测试方法

推荐使用 gcc 编译器，我使用的是：

```
$ gcc --version
gcc (Debian 13.2.0-5) 13.2.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

### 运行优化后的代码

首先，开启 AVX2 flag ，在 `./programs/Makefile` 里的第39行（ `CFLAGS` 那一行）加上 `-mavx2` 

然后在项目根目录下运行以下命令：

```
make
cd programs
./probagen 20%
./fse -b -B1048576 proba.bin
```

可以看到，打印出来的压缩性能 630MB/s ，解压性能 1956MB/s （在 Intel Core i7 12700H上）：

```
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
- Blocks 1024 KB -
proba.bin         :   1048575 ->    473502 (45.16%),  630.4 MB/s , 1956.9 MB/s
```

### 运行优化前的代码 (baseline)

关闭 AVX2 flag ，在 `./programs/Makefile` 里的第39行（ `CFLAGS` 那一行）删除 `-mavx2` 

然后在项目根目录下运行以下命令：

```
make
cd programs
./probagen 20%
./fse -b -B1048576 proba.bin
```

可以看到，打印出来的压缩性能 695MB/s ，解压性能 586MB/s （在 Intel Core i7 12700H上）：

```
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
- Blocks 1024 KB -
proba.bin         :   1048575 ->    473447 (45.15%),  695.0 MB/s ,  586.9 MB/s
```

### 

## 详细测试日志

以下是优化后的测试 log （需要把 `./programs/Makefile` 里的 `CFLAGS` 那一行加上 `-mavx2`）：

```
(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast]
└─$ make
make -C programs fse
make[1]: Entering directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cc -I../lib -O3 -mavx2  -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef   bench.c commandline.c fileio.c xxhash.c zlibh.c ../lib/entropy_common.c ../lib/hist.c ../lib/fse_decompress.c ../lib/fse_compress.c ../lib/fseU16.c ../lib/huf_compress.c ../lib/huf_decompress.c -o fse
../lib/fse_decompress.c: In function ‘FSE_decompress_stream8_interleave2_AVX2’:
../lib/fse_decompress.c:244:5: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  244 |     U16 tl =  ((const FSE_DTableHeader*)dt)->tableLog;
      |     ^~~
../lib/fse_decompress.c:245:59: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  245 |     const FSE_decode_t* dtable = ((const FSE_decode_t*)(dt+1));
      |                                                        ~~~^~~
../lib/fse_decompress.c:257:23: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  257 |     uint32_t *p_len = (uint32_t*)src;
      |                       ^
../lib/fse_decompress.c:278:48: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  278 |         __m256i v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
      |                                                ^
../lib/fse_decompress.c:294:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  294 |             v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
      |                                            ^
../lib/fse_decompress.c:298:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  298 |             v8_ol = _mm256_i32gather_epi32((int*)dtable, v8_sl, 4);            // ol = dtable[sl]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |                                            ^
../lib/fse_decompress.c:298:13: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  298 |             v8_ol = _mm256_i32gather_epi32((int*)dtable, v8_sl, 4);            // ol = dtable[sl]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |             ^~~~~
../lib/fse_decompress.c:299:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  299 |             v8_oh = _mm256_i32gather_epi32((int*)dtable, v8_sh, 4);            // oh = dtable[sh]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |                                            ^
../lib/fse_decompress.c:299:13: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  299 |             v8_oh = _mm256_i32gather_epi32((int*)dtable, v8_sh, 4);            // oh = dtable[sh]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |             ^~~~~
../lib/fse_decompress.c:243:128: warning: unused parameter ‘srcSize’ [-Wunused-parameter]
  243 | FORCE_INLINE_TEMPLATE size_t FSE_decompress_stream8_interleave2_AVX2 (uint8_t* dst, size_t dstSize, const uint8_t* src, size_t srcSize, const FSE_DTable* dt) {
      |                                                                                                                         ~~~~~~~^~~~~~~
../lib/fse_compress.c: In function ‘FSE_compress_stream8_interleave2’:
../lib/fse_compress.c:640:26: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  640 |     uint8_t *p_src     = (uint8_t*)src;
      |                          ^
../lib/fse_compress.c: At top level:
../lib/fse_compress.c:554:15: warning: ‘FSE_compress_usingCTable_generic’ defined but not used [-Wunused-function]
  554 | static size_t FSE_compress_usingCTable_generic (void* dst, size_t dstSize,
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
make[1]: Leaving directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cp programs/fse .
make -C programs check
make[1]: Entering directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cc -I../lib -O3 -mavx2  -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef   probaGenerator.c -o probagen
./probagen 20%
Binary file generator
Generating 1023 KB with P=20.00%
File proba.bin generated
**** compress using FSE ****
./fse -f proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 475984 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
**** compress using HUF ****
./fse -fh proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 478412 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
**** compress using zlibh ****
./fse -fz proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 478213 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
rm result
rm proba.bin
rm tmp
make[1]: Leaving directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast]
└─$ cd programs/

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs]
└─$ ./probagen 20%
Binary file generator
Generating 1023 KB with P=20.00%
File proba.bin generated

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs]
└─$ ./fse -b -B1048576 proba.bin
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
- Blocks 1024 KB -
proba.bin         :   1048575 ->    473502 (45.16%),  630.4 MB/s , 1956.9 MB/s

```

以下是 baseline (未优化前) 的测试 log （需要把 ./programs/Makefile 里的 `CFLAGS` 那一行的 `-mavx2` 删除）：

```
(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast]
└─$ make
make -C programs fse
make[1]: Entering directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cc -I../lib -O3   -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef   bench.c commandline.c fileio.c xxhash.c zlibh.c ../lib/entropy_common.c ../lib/hist.c ../lib/fse_decompress.c ../lib/fse_compress.c ../lib/fseU16.c ../lib/huf_compress.c ../lib/huf_decompress.c -o fse
../lib/fse_decompress.c: In function ‘FSE_decompress_stream8_interleave2_AVX2’:
../lib/fse_decompress.c:244:5: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  244 |     U16 tl =  ((const FSE_DTableHeader*)dt)->tableLog;
      |     ^~~
../lib/fse_decompress.c:245:59: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  245 |     const FSE_decode_t* dtable = ((const FSE_decode_t*)(dt+1));
      |                                                        ~~~^~~
../lib/fse_decompress.c:257:23: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  257 |     uint32_t *p_len = (uint32_t*)src;
      |                       ^
../lib/fse_decompress.c:278:48: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  278 |         __m256i v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
      |                                                ^
../lib/fse_decompress.c:294:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  294 |             v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
      |                                            ^
../lib/fse_decompress.c:298:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  298 |             v8_ol = _mm256_i32gather_epi32((int*)dtable, v8_sl, 4);            // ol = dtable[sl]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |                                            ^
../lib/fse_decompress.c:298:13: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  298 |             v8_ol = _mm256_i32gather_epi32((int*)dtable, v8_sl, 4);            // ol = dtable[sl]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |             ^~~~~
../lib/fse_decompress.c:299:44: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  299 |             v8_oh = _mm256_i32gather_epi32((int*)dtable, v8_sh, 4);            // oh = dtable[sh]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |                                            ^
../lib/fse_decompress.c:299:13: warning: dereferencing type-punned pointer might break strict-aliasing rules [-Wstrict-aliasing]
  299 |             v8_oh = _mm256_i32gather_epi32((int*)dtable, v8_sh, 4);            // oh = dtable[sh]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
      |             ^~~~~
../lib/fse_decompress.c:243:128: warning: unused parameter ‘srcSize’ [-Wunused-parameter]
  243 | FORCE_INLINE_TEMPLATE size_t FSE_decompress_stream8_interleave2_AVX2 (uint8_t* dst, size_t dstSize, const uint8_t* src, size_t srcSize, const FSE_DTable* dt) {
      |                                                                                                                         ~~~~~~~^~~~~~~
../lib/fse_compress.c: In function ‘FSE_compress_stream8_interleave2’:
../lib/fse_compress.c:640:26: warning: cast discards ‘const’ qualifier from pointer target type [-Wcast-qual]
  640 |     uint8_t *p_src     = (uint8_t*)src;
      |                          ^
../lib/fse_compress.c: At top level:
../lib/fse_compress.c:636:15: warning: ‘FSE_compress_stream8_interleave2’ defined but not used [-Wunused-function]
  636 | static size_t FSE_compress_stream8_interleave2 (void* dst, size_t dstSize, const void* src, size_t srcSize, const FSE_CTable* ct) {
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
make[1]: Leaving directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cp programs/fse .
make -C programs check
make[1]: Entering directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'
cc -I../lib -O3   -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wstrict-aliasing=1 -Wswitch-enum -Wstrict-prototypes -Wundef   probaGenerator.c -o probagen
./probagen 20%
Binary file generator
Generating 1023 KB with P=20.00%
File proba.bin generated
**** compress using FSE ****
./fse -f proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 474414 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
**** compress using HUF ****
./fse -fh proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 478412 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
**** compress using zlibh ****
./fse -fz proba.bin tmp
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Compressed 1048575 bytes into 478213 bytes ==> 0.00%
./fse -df tmp result
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
Decoded 1048575 bytes
diff proba.bin result
rm result
rm proba.bin
rm tmp
make[1]: Leaving directory '/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs'

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast]
└─$ cd programs/

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs]
└─$ ./probagen 20%
Binary file generator
Generating 1023 KB with P=20.00%
File proba.bin generated

(base) ┌──(wgg㉿DESKTOP-8K1DQI5)-[/mnt/d/Compress_Data/FiniteStateEntropy-Fast/programs]
└─$ ./fse -b -B1048576 proba.bin
FSE : Finite State Entropy, 64-bits demo by Yann Collet (Nov  1 2025)
- Blocks 1024 KB -
proba.bin         :   1048575 ->    473447 (45.15%),  695.0 MB/s ,  586.9 MB/s

```







New Generation Entropy coders
=============================

This library proposes two high speed entropy coders :

__Huff0__, a [Huffman codec](https://en.wikipedia.org/wiki/Huffman_coding) designed for modern CPU, 
featuring OoO (Out of Order) operations on multiple ALU (Arithmetic Logic Unit),
achieving extremely fast compression and decompression speeds.

__FSE__ is a new kind of [Entropy encoder](http://en.wikipedia.org/wiki/Entropy_encoding),
based on [ANS theory, from Jarek Duda](http://arxiv.org/abs/1311.2540),
achieving precise compression accuracy (like [Arithmetic coding](http://en.wikipedia.org/wiki/Arithmetic_coding)) at much higher speeds.

|Branch      |Status   |
|------------|---------|
|master      | [![Build Status](https://travis-ci.org/Cyan4973/FiniteStateEntropy.svg?branch=master)](https://travis-ci.org/Cyan4973/FiniteStateEntropy) |
|dev         | [![Build Status](https://travis-ci.org/Cyan4973/FiniteStateEntropy.svg?branch=dev)](https://travis-ci.org/Cyan4973/FiniteStateEntropy) |


Benchmarks
-------------------------

Benchmarks are run on an Intel Core i7-5600U, with Linux Mint 64-bits.
Source code is compiled using GCC 4.8.4, 64-bits mode.
Test files are generated using the provided `probagen` program.
Benchmark breaks sample files into blocks of 32 KB.
`Huff0` and `FSE` are compared to `zlibh`, the huffman encoder within zlib, provided by Frederic Kayser.

| File    | Codec | Ratio  | Compression | Decompression |
| ------- | ----- |:------:| -----------:| -------------:|
| Proba80 |       |        |             |               |
|         | Huff0 |  6.38  |__600 MB/s__ |__1350 MB/s__  |
|         | FSE   |__8.84__|  325 MB/s   |   440 MB/s    |
|         | zlibh |  6.38  |  265 MB/s   |   300 MB/s    |
| Proba14 |       |        |             |               |
|         | Huff0 |  1.90  |  595 MB/s   |   860 MB/s    |
|         | FSE   |  1.91  |  330 MB/s   |   460 MB/s    |
|         | zlibh |  1.90  |  255 MB/s   |   250 MB/s    |
| Proba02 |       |        |             |               |
|         | Huff0 |  1.13  |  525 MB/s   |   555 MB/s    |
|         | FSE   |  1.13  |  325 MB/s   |   445 MB/s    |
|         | zlibh |  1.13  |  180 MB/s   |   210 MB/s    |

By design, Huffman can't break the "1 bit per symbol" limit, hence loses efficiency on squeezed distributions, such as `Proba80`.
FSE is free of such limit, and its compression efficiency remains close to Shannon limit in all circumstances.
However, this accuracy is not always necessary, and less compressible distributions show little difference with Huffman.
On its side, Huff0 delivers in the form of a massive speed advantage.

Branch Policy
-------------------------
External contributions are welcomed and encouraged.
The "master" branch is only meant to host stable releases.
The "dev" branch is the one where all contributions are merged. If you want to propose a patch, please commit into "dev" branch or dedicated feature branch. Direct commit to "master" are not permitted.

