/**
 * zling:
 *  light-weight lossless data compression utility.
 *
 * Copyright (C) 2012-2013 by Zhang Li <zhangli10 at baidu.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @author zhangli10<zhangli10@baidu.com>
 * @brief  manipulate ROLZ (reduced offset Lempel-Ziv) compression.
 */
#include "libzling_lz.h"

namespace baidu {
namespace zling {
namespace lz {

static inline uint32_t HashContext(unsigned char* ptr) __attribute__((pure));
static inline uint32_t RollingAdd(uint32_t x, uint32_t y) __attribute__((pure));
static inline uint32_t RollingSub(uint32_t x, uint32_t y) __attribute__((pure));

static inline uint32_t HashContext(unsigned char* ptr) {
    return (*reinterpret_cast<uint32_t*>(ptr) + ptr[2] * 137 + ptr[3] * 13337);
}

static inline uint32_t RollingAdd(uint32_t x, uint32_t y) {
    return (x + y) & (kBucketItemSize - 1);
}
static inline uint32_t RollingSub(uint32_t x, uint32_t y) {
    return (x - y) & (kBucketItemSize - 1);
}

static inline int GetCommonLength(unsigned char* buf1, unsigned char* buf2, int maxlen) {
    unsigned char* p1 = buf1;
    unsigned char* p2 = buf2;

    while ((maxlen--) > 0 && *p1 == *p2) {
        p1++;
        p2++;
    }
    return p1 - buf1;
}

static inline void IncrementalCopyFastPath(unsigned char* src, unsigned char* dst, int len) {
    while (dst - src < 8) {
        *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<uint64_t*>(src);
        len -= dst - src;
        dst += dst - src;
    }
    while (len > 0) {
        *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<uint64_t*>(src);
        len -= 8;
        dst += 8;
        src += 8;
    }
    return;
}

int ZlingRolzEncoder::Encode(unsigned char* ibuf, uint16_t* obuf, int ilen, int olen, int* encpos) {
    int ipos = encpos[0];
    int opos = 0;

    // first byte
    if (ipos == 0 && opos < olen && ipos < ilen) {
        obuf[opos++] = ibuf[ipos++];
    }

    while (opos + 1 < olen && ipos + kMatchMaxLen < ilen) {
        int match_idx;
        int match_len;

        if (MatchAndUpdate(ibuf, ipos, &match_idx, &match_len, m_match_depth)) {
            obuf[opos++] = 256 + match_len - kMatchMinLen;  // encode as match
            obuf[opos++] = match_idx;
            ipos += match_len;

        } else {
            obuf[opos++] = ibuf[ipos];  // encode as literal
            ipos += 1;
        }
    }

    // rest byte
    while (opos < olen && ipos < ilen) {
        obuf[opos++] = ibuf[ipos];
        MatchAndUpdate(ibuf, ipos, NULL, NULL, 0);
        ipos += 1;
    }
    encpos[0] = ipos;
    return opos;
}

void ZlingRolzEncoder::Reset() {
    memset(m_buckets, 0, sizeof(m_buckets));
    return;
}

int inline ZlingRolzEncoder::MatchAndUpdate(unsigned char* buf, int pos, int* match_idx, int* match_len, int match_depth) {
    int maxlen = kMatchMinLen - 1;
    int maxidx = 0;
    uint32_t hash = HashContext(buf + pos);
    uint8_t  hash_check   = hash / kBucketItemHash % 256;
    uint32_t hash_context = hash % kBucketItemHash;

    int node;
    int i;
    ZlingEncodeBucket* bucket = &m_buckets[buf[pos - 1]];

    node = bucket->hash[hash_context];

    // update befault matching (to make it faster)
    bucket->head = RollingAdd(bucket->head, 1);
    bucket->suffix[bucket->head] = bucket->hash[hash_context];
    bucket->offset[bucket->head] = pos | hash_check << 24;
    bucket->hash[hash_context] = bucket->head;

    // no match for first position (faster)
    if (node == 0) {
        return 0;
    }

    // entry already updated, cannot match
    if (node == bucket->head) {
        return 0;
    }

    // start matching
    for (i = 0; i < match_depth; i++) {
        uint32_t offset = bucket->offset[node] & 0xffffff;
        uint8_t  check = bucket->offset[node] >> 24;

        if (check == hash_check && buf[pos + maxlen] == buf[offset + maxlen]) {
            int len = GetCommonLength(buf + pos, buf + offset, kMatchMaxLen);

            if (len > maxlen) {
                maxidx = RollingSub(bucket->head, node);
                maxlen = len;
                if (maxlen == kMatchMaxLen) {
                    break;
                }
            }
        }
        node = bucket->suffix[node];

        // end chaining?
        if (!node || offset <= (bucket->offset[node] & 0xffffff)) {
            break;
        }
    }

    if (maxlen >= kMatchMinLen + (maxidx >= kMatchDiscardMinLen)) {
        if (maxlen < kMatchMinLenEnableLazy) {  // fast and stupid lazy parsing
            if (m_lazymatch1_depth > 0 && MatchLazy(buf, pos + 1, maxlen, m_lazymatch1_depth)) {
                return 0;
            }
            if (m_lazymatch2_depth > 0 && MatchLazy(buf, pos + 2, maxlen, m_lazymatch2_depth)) {
                return 0;
            }
        }
        match_len[0] = maxlen;
        match_idx[0] = maxidx;
        return 1;
    }
    return 0;
}

int inline ZlingRolzEncoder::MatchLazy(unsigned char* buf, int pos, int maxlen, int depth) {
    ZlingEncodeBucket* bucket = &m_buckets[buf[pos - 1]];
    int node = bucket->hash[HashContext(buf + pos) % kBucketItemHash];

    uint32_t fetch_current = *reinterpret_cast<uint32_t*>(buf + pos + maxlen - 3);
    uint32_t fetch_match;

    for (int i = 0; i < depth; i++) {
        fetch_match = *reinterpret_cast<uint32_t*>(buf + (bucket->offset[node] & 0xffffff) + maxlen - 3);

        if (fetch_current == fetch_match) {
            return 1;
        }
        node = bucket->suffix[node];
    }
    return 0;
}

int ZlingRolzDecoder::Decode(uint16_t* ibuf, unsigned char* obuf, int ilen, int encpos, int* decpos) {
    int opos = decpos[0];
    int ipos = 0;
    int match_idx;
    int match_len;
    int match_offset;

    // first byte
    if (opos == 0 && ipos < ilen) {
        obuf[opos++] = ibuf[ipos++];
    }

    // rest byte
    while (ipos < ilen) {
        if (ibuf[ipos] < 256) {  // process a literal byte
            obuf[opos] = ibuf[ipos++];
            GetMatchAndUpdate(obuf, opos, match_idx);
            opos++;

        } else {  // process a match
            match_len = ibuf[ipos++] - 256 + kMatchMinLen;
            match_idx = ibuf[ipos++];
            match_offset = GetMatchAndUpdate(obuf, opos, match_idx);

            IncrementalCopyFastPath(&obuf[match_offset], &obuf[opos], match_len);
            opos += match_len;
        }

        if (opos > encpos) {
            return -1;
        }
    }

    if (opos != encpos) {
        return -1;
    }
    decpos[0] = opos;
    return 0;
}

void ZlingRolzDecoder::Reset() {
    memset(m_buckets, 0, sizeof(m_buckets));
    return;
}

int inline ZlingRolzDecoder::GetMatchAndUpdate(unsigned char* buf, int pos, int idx) {
    ZlingDecodeBucket* bucket = &m_buckets[buf[pos - 1]];
    int node;

    // update
    bucket->head = RollingAdd(bucket->head, 1);
    bucket->offset[bucket->head] = pos;

    // get match
    node = RollingSub(bucket->head, idx);
    return bucket->offset[node];
}

}  // namespace lz
}  // namespace zling
}  // namespace baidu
