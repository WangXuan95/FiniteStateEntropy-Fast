/* ******************************************************************
 * FSE : Finite State Entropy decoder
 * Copyright (c) 2013-2020, Yann Collet, Facebook, Inc.
 *
 *  You can contact the author at :
 *  - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *  - Public forum : https://groups.google.com/forum/#!forum/lz4c
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */


/* **************************************************************
*  Includes
****************************************************************/
#include <stdlib.h>     /* malloc, free, qsort */
#include <string.h>     /* memcpy, memset */
#include "debug.h"      /* assert */
#include "bitstream.h"
#include "compiler.h"
#define FSE_STATIC_LINKING_ONLY
#include "fse.h"
#include "error_private.h"

#include <immintrin.h>  // AVX2


/* **************************************************************
*  Error Management
****************************************************************/
#define FSE_isError ERR_isError
#define FSE_STATIC_ASSERT(c) DEBUG_STATIC_ASSERT(c)   /* use only *after* variable declarations */


/* **************************************************************
*  Templates
****************************************************************/
/*
  designed to be included
  for type-specific functions (template emulation in C)
  Objective is to write these functions only once, for improved maintenance
*/

/* safety checks */
#ifndef FSE_FUNCTION_EXTENSION
#  error "FSE_FUNCTION_EXTENSION must be defined"
#endif
#ifndef FSE_FUNCTION_TYPE
#  error "FSE_FUNCTION_TYPE must be defined"
#endif

/* Function names */
#define FSE_CAT(X,Y) X##Y
#define FSE_FUNCTION_NAME(X,Y) FSE_CAT(X,Y)
#define FSE_TYPE_NAME(X,Y) FSE_CAT(X,Y)


/* Function templates */
FSE_DTable* FSE_createDTable (unsigned tableLog)
{
    if (tableLog > FSE_TABLELOG_ABSOLUTE_MAX) tableLog = FSE_TABLELOG_ABSOLUTE_MAX;
    return (FSE_DTable*)malloc( FSE_DTABLE_SIZE_U32(tableLog) * sizeof (U32) );
}

void FSE_freeDTable (FSE_DTable* dt)
{
    free(dt);
}

size_t FSE_buildDTable(FSE_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog)
{
    void* const tdPtr = dt+1;   /* because *dt is unsigned, 32-bits aligned on 32-bits */
    FSE_DECODE_TYPE* const tableDecode = (FSE_DECODE_TYPE*) (tdPtr);
    U16 symbolNext[FSE_MAX_SYMBOL_VALUE+1];

    U32 const maxSV1 = maxSymbolValue + 1;
    U32 const tableSize = 1 << tableLog;
    U32 highThreshold = tableSize-1;

    /* Sanity Checks */
    if (maxSymbolValue > FSE_MAX_SYMBOL_VALUE) return ERROR(maxSymbolValue_tooLarge);
    if (tableLog > FSE_MAX_TABLELOG) return ERROR(tableLog_tooLarge);

    /* Init, lay down lowprob symbols */
    {   FSE_DTableHeader DTableH;
        DTableH.tableLog = (U16)tableLog;
        DTableH.fastMode = 1;
        {   S16 const largeLimit= (S16)(1 << (tableLog-1));
            U32 s;
            for (s=0; s<maxSV1; s++) {
                if (normalizedCounter[s]==-1) {
                    tableDecode[highThreshold--].symbol = (FSE_FUNCTION_TYPE)s;
                    symbolNext[s] = 1;
                } else {
                    if (normalizedCounter[s] >= largeLimit) DTableH.fastMode=0;
                    symbolNext[s] = normalizedCounter[s];
        }   }   }
        memcpy(dt, &DTableH, sizeof(DTableH));
    }

    /* Spread symbols */
    {   U32 const tableMask = tableSize-1;
        U32 const step = FSE_TABLESTEP(tableSize);
        U32 s, position = 0;
        for (s=0; s<maxSV1; s++) {
            int i;
            for (i=0; i<normalizedCounter[s]; i++) {
                tableDecode[position].symbol = (FSE_FUNCTION_TYPE)s;
                position = (position + step) & tableMask;
                while (position > highThreshold) position = (position + step) & tableMask;   /* lowprob area */
        }   }
        if (position!=0) return ERROR(GENERIC);   /* position must reach all cells once, otherwise normalizedCounter is incorrect */
    }

    /* Build Decoding table */
    {   U32 u;
        for (u=0; u<tableSize; u++) {
            FSE_FUNCTION_TYPE const symbol = (FSE_FUNCTION_TYPE)(tableDecode[u].symbol);
            U32 const nextState = symbolNext[symbol]++;
            tableDecode[u].nbBits = (BYTE) (tableLog - BIT_highbit32(nextState) );
            tableDecode[u].newState = (U16) ( (nextState << tableDecode[u].nbBits) - tableSize);
    }   }

    return 0;
}


#ifndef FSE_COMMONDEFS_ONLY

/*-*******************************************************
*  Decompression (Byte symbols)
*********************************************************/
size_t FSE_buildDTable_rle (FSE_DTable* dt, BYTE symbolValue)
{
    void* ptr = dt;
    FSE_DTableHeader* const DTableH = (FSE_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSE_decode_t* const cell = (FSE_decode_t*)dPtr;

    DTableH->tableLog = 0;
    DTableH->fastMode = 0;

    cell->newState = 0;
    cell->symbol = symbolValue;
    cell->nbBits = 0;

    return 0;
}


size_t FSE_buildDTable_raw (FSE_DTable* dt, unsigned nbBits)
{
    void* ptr = dt;
    FSE_DTableHeader* const DTableH = (FSE_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSE_decode_t* const dinfo = (FSE_decode_t*)dPtr;
    const unsigned tableSize = 1 << nbBits;
    const unsigned tableMask = tableSize - 1;
    const unsigned maxSV1 = tableMask+1;
    unsigned s;

    /* Sanity checks */
    if (nbBits < 1) return ERROR(GENERIC);         /* min size */

    /* Build Decoding Table */
    DTableH->tableLog = (U16)nbBits;
    DTableH->fastMode = 1;
    for (s=0; s<maxSV1; s++) {
        dinfo[s].newState = 0;
        dinfo[s].symbol = (BYTE)s;
        dinfo[s].nbBits = (BYTE)nbBits;
    }

    return 0;
}

FORCE_INLINE_TEMPLATE size_t FSE_decompress_usingDTable_generic(
          void* dst, size_t maxDstSize,
    const void* cSrc, size_t cSrcSize,
    const FSE_DTable* dt, const unsigned fast)
{
    BYTE* const ostart = (BYTE*) dst;
    BYTE* op = ostart;
    BYTE* const omax = op + maxDstSize;
    BYTE* const olimit = omax-3;

    BIT_DStream_t bitD;
    FSE_DState_t state1;
    FSE_DState_t state2;

    /* Init */
    CHECK_F(BIT_initDStream(&bitD, cSrc, cSrcSize));

    FSE_initDState(&state1, &bitD, dt);
    FSE_initDState(&state2, &bitD, dt);

#define FSE_GETSYMBOL(statePtr) fast ? FSE_decodeSymbolFast(statePtr, &bitD) : FSE_decodeSymbol(statePtr, &bitD)

    /* 4 symbols per loop */
    for ( ; (BIT_reloadDStream(&bitD)==BIT_DStream_unfinished) & (op<olimit) ; op+=4) {
        op[0] = FSE_GETSYMBOL(&state1);

        if (FSE_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[1] = FSE_GETSYMBOL(&state2);

        if (FSE_MAX_TABLELOG*4+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            { if (BIT_reloadDStream(&bitD) > BIT_DStream_unfinished) { op+=2; break; } }

        op[2] = FSE_GETSYMBOL(&state1);

        if (FSE_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[3] = FSE_GETSYMBOL(&state2);
    }

    /* tail */
    /* note : BIT_reloadDStream(&bitD) >= FSE_DStream_partiallyFilled; Ends at exactly BIT_DStream_completed */
    while (1) {
        if (op>(omax-2)) return ERROR(dstSize_tooSmall);
        *op++ = FSE_GETSYMBOL(&state1);
        if (BIT_reloadDStream(&bitD)==BIT_DStream_overflow) {
            *op++ = FSE_GETSYMBOL(&state2);
            break;
        }

        if (op>(omax-2)) return ERROR(dstSize_tooSmall);
        *op++ = FSE_GETSYMBOL(&state2);
        if (BIT_reloadDStream(&bitD)==BIT_DStream_overflow) {
            *op++ = FSE_GETSYMBOL(&state1);
            break;
    }   }

    return op-ostart;
}


FORCE_INLINE_TEMPLATE size_t FSE_decompress_stream8_interleave2_AVX2 (uint8_t* dst, size_t dstSize, const uint8_t* src, size_t srcSize, const FSE_DTable* dt) {
    U16 tl =  ((const FSE_DTableHeader*)dt)->tableLog;
    const FSE_decode_t* dtable = ((const FSE_decode_t*)(dt+1));

    uint8_t* op = dst;
    uint8_t* const omax = op + dstSize;
    
    size_t size_rem = *(src++);
    if (size_rem > 0) {
        memcpy(op, src, size_rem);
        src += size_rem;
        op  += size_rem;
    }

    uint32_t *p_len = (uint32_t*)src;
    if (p_len[0] > 0) {
        uint8_t *ip0 = (uint8_t*)(&(p_len[8])) + p_len[0] - 4;    // 一次加载一个 uint32_t , 因此这里是 - 4
        uint8_t *ip1 = ip0                     + p_len[1];
        uint8_t *ip2 = ip1                     + p_len[2];
        uint8_t *ip3 = ip2                     + p_len[3];
        uint8_t *ip4 = ip3                     + p_len[4];
        uint8_t *ip5 = ip4                     + p_len[5];
        uint8_t *ip6 = ip5                     + p_len[6];
        uint8_t *ip7 = ip6                     + p_len[7];

        __m256i v8_ip = _mm256_set_epi32((ip7-src), (ip6-src), (ip5-src), (ip4-src), (ip3-src), (ip2-src), (ip1-src), (ip0-src));
        __m256i v8_c  = _mm256_setr_epi32(
            ip0[3] ? (8 - BIT_highbit32(ip0[3])) : 0,
            ip1[3] ? (8 - BIT_highbit32(ip1[3])) : 0,
            ip2[3] ? (8 - BIT_highbit32(ip2[3])) : 0,
            ip3[3] ? (8 - BIT_highbit32(ip3[3])) : 0,
            ip4[3] ? (8 - BIT_highbit32(ip4[3])) : 0,
            ip5[3] ? (8 - BIT_highbit32(ip5[3])) : 0,
            ip6[3] ? (8 - BIT_highbit32(ip6[3])) : 0,
            ip7[3] ? (8 - BIT_highbit32(ip7[3])) : 0 );
        __m256i v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
        __m256i v8_sl = _mm256_srlv_epi32(_mm256_sllv_epi32(v8_d,                  v8_c                        ), _mm256_set1_epi32(32-tl));
        __m256i v8_sh = _mm256_srlv_epi32(_mm256_sllv_epi32(v8_d, _mm256_add_epi32(v8_c, _mm256_set1_epi32(tl))), _mm256_set1_epi32(32-tl));
        v8_c = _mm256_add_epi32(v8_c, _mm256_set1_epi32(tl*2));

        __m256i v8_7  = _mm256_set1_epi32(7);
        __m256i v8_32 = _mm256_set1_epi32(32);
        __m256i v8_FFFF = _mm256_set1_epi32(0xFFFF);
        __m256i v16_FF = _mm256_set1_epi16(0xFF);

        for (; op<omax; op+=16) {
            __m256i v8_ol, v8_oh, v16_o, v8_nbl, v8_nbh;

            // reload ---------------------------------------------------------------------------------------------------------------
            v8_ip = _mm256_sub_epi32(v8_ip, _mm256_srli_epi32(v8_c, 3));
            v8_c  = _mm256_and_si256(v8_c, v8_7);
            v8_d  = _mm256_i32gather_epi32((int*)src, v8_ip, 1);
            v8_d  = _mm256_sllv_epi32(v8_d, v8_c);
            
            // decode 16 bytes ---------------------------------------------------------------------------------------------------------------
            v8_ol = _mm256_i32gather_epi32((int*)dtable, v8_sl, 4);            // ol = dtable[sl]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
            v8_oh = _mm256_i32gather_epi32((int*)dtable, v8_sh, 4);            // oh = dtable[sh]      {8-bit nbBits, 8-bit symbol, 16-bit newState}
            v8_nbl= _mm256_srli_epi32(v8_ol, 24);                              // nbl= (ol>>24)
            v8_nbh= _mm256_srli_epi32(v8_oh, 24);                              // nbh= (oh>>24)
            v8_sl = _mm256_srlv_epi32(v8_d, _mm256_sub_epi32(v8_32, v8_nbl));  // sl = d>>(32-nbl)
            v8_sl = _mm256_add_epi32(v8_sl, _mm256_and_si256(v8_ol, v8_FFFF)); // sl+= ol&0xFFFF
            v8_sh = _mm256_srlv_epi32(_mm256_sllv_epi32(v8_d, v8_nbl), _mm256_sub_epi32(v8_32, v8_nbh));  // sl = (d<<nbl)>>(32-nbh)
            v8_sh = _mm256_add_epi32(v8_sh, _mm256_and_si256(v8_oh, v8_FFFF)); // sh+= oh&0xFFFF
            v8_c  = _mm256_add_epi32(v8_c, _mm256_add_epi32(v8_nbl, v8_nbh));  // c += nb
            v8_ol = _mm256_srli_epi32(v8_ol, 16);
            v8_oh = _mm256_srli_epi32(v8_oh, 16);
            v16_o = _mm256_permute4x64_epi64(_mm256_packus_epi32(v8_ol, v8_oh), _MM_SHUFFLE(3, 1, 2, 0));
            v16_o = _mm256_and_si256(v16_o, v16_FF);
            v16_o = _mm256_permute4x64_epi64(_mm256_packus_epi16(v16_o, v16_o), _MM_SHUFFLE(3, 1, 2, 0));
            _mm_storeu_si128((__m128i*)op, _mm256_castsi256_si128(v16_o));
        }
    }

    return op - (uint8_t*)dst;
}


size_t FSE_decompress_usingDTable(void* dst, size_t originalSize,
                            const void* cSrc, size_t cSrcSize,
                            const FSE_DTable* dt)
{
#ifdef __AVX2__
    return FSE_decompress_stream8_interleave2_AVX2(dst, originalSize, cSrc, cSrcSize, dt);
#else
    const void* ptr = dt;
    const FSE_DTableHeader* DTableH = (const FSE_DTableHeader*)ptr;
    const U32 fastMode = DTableH->fastMode;
    /* select fast mode (static) */
    if (fastMode) return FSE_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 1);
    return FSE_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 0);
#endif
}


size_t FSE_decompress_wksp(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, FSE_DTable* workSpace, unsigned maxLog)
{
    const BYTE* const istart = (const BYTE*)cSrc;
    const BYTE* ip = istart;
    short counting[FSE_MAX_SYMBOL_VALUE+1];
    unsigned tableLog;
    unsigned maxSymbolValue = FSE_MAX_SYMBOL_VALUE;

    /* normal FSE decoding mode */
    size_t const NCountLength = FSE_readNCount (counting, &maxSymbolValue, &tableLog, istart, cSrcSize);
    if (FSE_isError(NCountLength)) return NCountLength;
    if (tableLog > maxLog) return ERROR(tableLog_tooLarge);
    assert(NCountLength <= cSrcSize);
    ip += NCountLength;
    cSrcSize -= NCountLength;

    CHECK_F( FSE_buildDTable (workSpace, counting, maxSymbolValue, tableLog) );

    return FSE_decompress_usingDTable (dst, dstCapacity, ip, cSrcSize, workSpace);   /* always return, even if it is an error code */
}


typedef FSE_DTable DTable_max_t[FSE_DTABLE_SIZE_U32(FSE_MAX_TABLELOG)];

size_t FSE_decompress(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize)
{
    DTable_max_t dt;   /* Static analyzer seems unable to understand this table will be properly initialized later */
    return FSE_decompress_wksp(dst, dstCapacity, cSrc, cSrcSize, dt, FSE_MAX_TABLELOG);
}



#endif   /* FSE_COMMONDEFS_ONLY */
