/*

Copyright (c) 2026 Mike Brady 4265913+mikebrady@users.noreply.github.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Parses a binary property list ("bplist00") held as a byte buffer in
memory, builds a tree of PlistNode, and pretty-prints it recursively.
"Data" nodes get checked for embedded binary plists
and pretty-printed if so.

Binary plist format (summary):

  [ 8 bytes magic "bplist00" ]
  [ object table: each object variable-length, tagged ]
  [ offset table: N entries, each `offset_size` bytes,
    offset_table[i] = byte offset of object i in the object table ]
  [ 32-byte trailer, at the very end of the file:
      6 bytes  unused
      1 byte   sort version   (unused)
      1 byte   offset_size    (bytes per offset-table entry)
      1 byte   object_ref_size (bytes per object reference)
      8 bytes  num_objects    (big-endian)
      8 bytes  top_object     (index of the root object, big-endian)
      8 bytes  offset_table_offset (big-endian) ]

Object marker byte: high nibble = type, low nibble = extra info
(either a literal small count, or 0xF meaning "count follows as its
own int object"). See decode_object() for the type table.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "bplist-print.h"

/* ---------- Tree types (same shape as the earlier in-memory model) ---------- */

typedef enum {
    PLIST_DICT,
    PLIST_ARRAY,
    PLIST_SET,       /* rare; binary-plist-only container type */
    PLIST_STRING,
    PLIST_INTEGER,
    PLIST_REAL,
    PLIST_BOOLEAN,
    PLIST_DATE,
    PLIST_DATA,
    PLIST_UID,       /* NSKeyedArchiver object reference */
    PLIST_NULL
} PlistType;

typedef struct PlistNode PlistNode;

typedef struct PlistDictEntry {
    char *key;
    PlistNode *value;
    struct PlistDictEntry *next;
} PlistDictEntry;

struct PlistNode {
    PlistType type;
    union {
        struct { PlistDictEntry *head; }                dict;
        struct { PlistNode **items; size_t count; size_t cap; } array;
        char    *string;    /* PLIST_STRING */
        int64_t  integer;
        double   real;
        bool     boolean;
        double   date;      /* seconds since 2001-01-01T00:00:00Z (CFAbsoluteTime) */
        struct { char *bytes; size_t length; } data;
        uint64_t uid;
    } v;
};

static PlistNode *plist_new(PlistType type) {
    PlistNode *n = calloc(1, sizeof(PlistNode));
    if (!n) { perror("calloc"); exit(EXIT_FAILURE); }
    n->type = type;
    return n;
}

static PlistNode *plist_new_array_like(PlistType type) {
    PlistNode *n = plist_new(type);
    n->v.array.cap = 4;
    n->v.array.items = malloc(n->v.array.cap * sizeof(PlistNode *));
    return n;
}

static void plist_array_add(PlistNode *array, PlistNode *item) {
    if (array->v.array.count == array->v.array.cap) {
        array->v.array.cap *= 2;
        array->v.array.items = realloc(array->v.array.items,
                                        array->v.array.cap * sizeof(PlistNode *));
    }
    array->v.array.items[array->v.array.count++] = item;
}

static void plist_dict_set(PlistNode *dict, const char *key, PlistNode *value) {
    PlistDictEntry *e = malloc(sizeof(PlistDictEntry));
    e->key = strdup(key);
    e->value = value;
    e->next = dict->v.dict.head;
    dict->v.dict.head = e;
}

void plist_free(PlistNode *node) {
    if (!node) return;
    switch (node->type) {
    case PLIST_DICT:
        for (PlistDictEntry *e = node->v.dict.head; e; ) {
            PlistDictEntry *next = e->next;
            free(e->key);
            plist_free(e->value);
            free(e);
            e = next;
        }
        break;
    case PLIST_ARRAY:
    case PLIST_SET:
        for (size_t i = 0; i < node->v.array.count; i++)
            plist_free(node->v.array.items[i]);
        free(node->v.array.items);
        break;
    case PLIST_STRING:
        free(node->v.string);
        break;
    case PLIST_DATA:
        free(node->v.data.bytes);
        break;
    default:
        break;
    }
    free(node);
}

/* ---------- Pretty printer ---------- */

static void indent(int depth) {
    for (int i = 0; i < depth; i++) printf("    ");
}

#define PLIST_DATA_MAX_DISPLAY_BYTES 64

static void plist_print_data(const PlistNode *node, int depth) {
    size_t len = node->v.data.length;
    size_t show = len > PLIST_DATA_MAX_DISPLAY_BYTES ? PLIST_DATA_MAX_DISPLAY_BYTES : len;

    if ((len > strlen("bplist00")) && (strncmp(node->v.data.bytes, "bplist00", strlen("bplist00")) == 0)) {
        printf("<bplist in a Data node, %zu byte%s>\n", len, len == 1 ? "" : "s");
        pretty_print_binary_plist(node->v.data.bytes, len, depth);
    } else {
      printf("<Data, %zu byte%s>\n", len, len == 1 ? "" : "s");
      for (size_t off = 0; off < show; off += 16) {
          size_t line_len = (show - off < 16) ? (show - off) : 16;
          indent(depth);
          printf("%08zx  ", off);
          for (size_t i = 0; i < 16; i++) {
              if (i < line_len) printf("%02x ", node->v.data.bytes[off + i]);
              else printf("   ");
              if (i == 7) printf(" ");
          }
          printf(" |");
          for (size_t i = 0; i < line_len; i++) {
              unsigned char c = node->v.data.bytes[off + i];
              putchar((c >= 32 && c < 127) ? c : '.');
          }
          printf("|\n");
      }
      if (len > show) {
          indent(depth);
          printf("... (%zu more byte%s truncated)\n", len - show, (len - show) == 1 ? "" : "s");
      }
    }
}

static void plist_print_date(double cf_abs_time) {
    /* CFAbsoluteTime is seconds relative to 2001-01-01T00:00:00Z.
     * Convert to a Unix time_t and format as ISO 8601 for display. */
    const time_t epoch_delta = 978307200; /* seconds between 1970-01-01 and 2001-01-01 */
    time_t unix_time = (time_t)cf_abs_time + epoch_delta;
    struct tm tm_utc;
    gmtime_r(&unix_time, &tm_utc);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    printf("<Date: %s>\n", buf);
}

void plist_print(const PlistNode *node, int depth) {
    if (!node) { printf("<null>\n"); return; }

    switch (node->type) {
    case PLIST_DICT: {
        printf("{\n");
        for (PlistDictEntry *e = node->v.dict.head; e; e = e->next) {
            indent(depth + 1);
            printf("%s: ", e->key);
            plist_print(e->value, depth + 1);
        }
        indent(depth);
        printf("}\n");
        break;
    }
    case PLIST_ARRAY:
    case PLIST_SET: {
        printf(node->type == PLIST_SET ? "(\n" : "[\n");
        for (size_t i = 0; i < node->v.array.count; i++) {
            indent(depth + 1);
            plist_print(node->v.array.items[i], depth + 1);
        }
        indent(depth);
        printf(node->type == PLIST_SET ? ")\n" : "]\n");
        break;
    }
    case PLIST_STRING:
        printf("\"%s\"\n", node->v.string);
        break;
    case PLIST_INTEGER:
        printf("%lld\n", (long long)node->v.integer);
        break;
    case PLIST_REAL:
        printf("%g\n", node->v.real);
        break;
    case PLIST_BOOLEAN:
        printf("%s\n", node->v.boolean ? "true" : "false");
        break;
    case PLIST_DATE:
        plist_print_date(node->v.date);
        break;
    case PLIST_DATA:
        plist_print_data(node, depth + 1);
        break;
    case PLIST_UID:
        printf("<UID: %llu>\n", (unsigned long long)node->v.uid);
        break;
    case PLIST_NULL:
        printf("null\n");
        break;
    }
}

/* ---------- Binary plist parser ---------- */

typedef struct {
    const char *buf;
    size_t len;
    uint8_t  offset_size;
    uint8_t  object_ref_size;
    uint64_t num_objects;
    uint64_t top_object;
    uint64_t offset_table_offset;
    const char *offset_table; /* points into buf */
} BplistCtx;

static uint64_t read_be_uint(const char *p, size_t nbytes) {
    uint64_t v = 0;
    for (size_t i = 0; i < nbytes; i++) v = (v << 8) | p[i];
    return v;
}

static double read_be_float(const char *p, size_t nbytes) {
    if (nbytes == 4) {
        uint32_t bits = (uint32_t)read_be_uint(p, 4);
        float f; memcpy(&f, &bits, 4);
        return (double)f;
    } else {
        uint64_t bits = read_be_uint(p, 8);
        double d; memcpy(&d, &bits, 8);
        return d;
    }
}

static uint64_t object_offset(const BplistCtx *ctx, uint64_t index) {
    return read_be_uint(ctx->offset_table + index * ctx->offset_size, ctx->offset_size);
}

/* Reads the "size" of a sized object (string/data/array/dict/set):
   if the marker's low nibble != 0xF, that nibble IS the count and the
   size encoding is just the 1 marker byte. If it's 0xF, the next byte
   starts a nested int object giving the real count.
   *out_header_bytes = total bytes consumed by marker+size encoding. */
static uint64_t read_size(const char *buf, size_t off, size_t *out_header_bytes) {
    uint8_t marker = buf[off];
    uint8_t info = marker & 0x0F;
    if (info != 0x0F) {
        *out_header_bytes = 1;
        return info;
    }
    uint8_t int_marker = buf[off + 1];
    size_t int_bytes = (size_t)1 << (int_marker & 0x0F);
    uint64_t count = read_be_uint(buf + off + 2, int_bytes);
    *out_header_bytes = 2 + int_bytes;
    return count;
}

static PlistNode *decode_object(BplistCtx *ctx, uint64_t index);

static PlistNode *decode_at(BplistCtx *ctx, size_t off) {
    uint8_t marker = ctx->buf[off];
    uint8_t type = marker >> 4;
    uint8_t info = marker & 0x0F;

    switch (type) {

    case 0x0: /* null / bool / fill */
        if (marker == 0x00) return plist_new(PLIST_NULL);
        if (marker == 0x08) { PlistNode *n = plist_new(PLIST_BOOLEAN); n->v.boolean = false; return n; }
        if (marker == 0x09) { PlistNode *n = plist_new(PLIST_BOOLEAN); n->v.boolean = true;  return n; }
        return plist_new(PLIST_NULL); /* 0x0F fill byte, shouldn't be decoded as an object */

    case 0x1: { /* int: info = log2(byte count) */
        size_t nbytes = (size_t)1 << info;
        PlistNode *n = plist_new(PLIST_INTEGER);
        if (nbytes >= 8) {
            /* 8-byte ints are signed two's complement; 16-byte "big" ints
             * are rare (huge integers) -- we only recover the low 64 bits. */
            uint64_t raw = read_be_uint(ctx->buf + off + 1 + (nbytes - 8), 8);
            n->v.integer = (int64_t)raw;
        } else {
            /* 1/2/4-byte ints are unsigned per the format. */
            n->v.integer = (int64_t)read_be_uint(ctx->buf + off + 1, nbytes);
        }
        return n;
    }

    case 0x2: { /* real: info = log2(byte count), 4 (float) or 8 (double) */
        size_t nbytes = (size_t)1 << info;
        PlistNode *n = plist_new(PLIST_REAL);
        n->v.real = read_be_float(ctx->buf + off + 1, nbytes);
        return n;
    }

    case 0x3: { /* date: always an 8-byte big-endian double */
        PlistNode *n = plist_new(PLIST_DATE);
        n->v.date = read_be_float(ctx->buf + off + 1, 8);
        return n;
    }

    case 0x4: { /* data */
        size_t header;
        uint64_t count = read_size(ctx->buf, off, &header);
        PlistNode *n = plist_new(PLIST_DATA);
        n->v.data.length = count;
        n->v.data.bytes = malloc(count);
        memcpy(n->v.data.bytes, ctx->buf + off + header, count);
        return n;
    }

    case 0x5: { /* ASCII string */
        size_t header;
        uint64_t count = read_size(ctx->buf, off, &header);
        PlistNode *n = plist_new(PLIST_STRING);
        n->v.string = malloc(count + 1);
        memcpy(n->v.string, ctx->buf + off + header, count);
        n->v.string[count] = '\0';
        return n;
    }

    case 0x6: { /* UTF-16BE string -- simplified: BMP characters only,
                 * converted to UTF-8. Surrogate pairs are not handled;
                 * good starting point, extend if you need full Unicode. */
        size_t header;
        uint64_t count = read_size(ctx->buf, off, &header);
        const char *p = ctx->buf + off + header;
        char *out = malloc(count * 3 + 1); /* worst case 3 bytes/UTF-8 char */
        size_t oi = 0;
        for (uint64_t i = 0; i < count; i++) {
            uint16_t cu = (uint16_t)read_be_uint(p + i * 2, 2);
            if (cu < 0x80) {
                out[oi++] = (char)cu;
            } else if (cu < 0x800) {
                out[oi++] = (char)(0xC0 | (cu >> 6));
                out[oi++] = (char)(0x80 | (cu & 0x3F));
            } else {
                out[oi++] = (char)(0xE0 | (cu >> 12));
                out[oi++] = (char)(0x80 | ((cu >> 6) & 0x3F));
                out[oi++] = (char)(0x80 | (cu & 0x3F));
            }
        }
        out[oi] = '\0';
        PlistNode *n = plist_new(PLIST_STRING);
        n->v.string = out;
        return n;
    }

    case 0x8: { /* UID: info+1 = byte count */
        size_t nbytes = (size_t)info + 1;
        PlistNode *n = plist_new(PLIST_UID);
        n->v.uid = read_be_uint(ctx->buf + off + 1, nbytes);
        return n;
    }

    case 0xA: /* array */
    case 0xC: { /* set */
        size_t header;
        uint64_t count = read_size(ctx->buf, off, &header);
        PlistNode *n = plist_new_array_like(type == 0xC ? PLIST_SET : PLIST_ARRAY);
        const char *refs = ctx->buf + off + header;
        for (uint64_t i = 0; i < count; i++) {
            uint64_t ref = read_be_uint(refs + i * ctx->object_ref_size, ctx->object_ref_size);
            plist_array_add(n, decode_object(ctx, ref));
        }
        return n;
    }

    case 0xD: { /* dict: `count` key refs, then `count` value refs */
        size_t header;
        uint64_t count = read_size(ctx->buf, off, &header);
        PlistNode *n = plist_new(PLIST_DICT);
        const char *key_refs = ctx->buf + off + header;
        const char *val_refs = key_refs + count * ctx->object_ref_size;
        for (uint64_t i = 0; i < count; i++) {
            uint64_t kref = read_be_uint(key_refs + i * ctx->object_ref_size, ctx->object_ref_size);
            uint64_t vref = read_be_uint(val_refs + i * ctx->object_ref_size, ctx->object_ref_size);
            PlistNode *key_node = decode_object(ctx, kref);
            const char *key_str = (key_node->type == PLIST_STRING) ? key_node->v.string : "<non-string key>";
            plist_dict_set(n, key_str, decode_object(ctx, vref));
            plist_free(key_node);
        }
        return n;
    }

    default:
        fprintf(stderr, "bplist: unknown object type marker 0x%02x at offset %zu\n", marker, off);
        return plist_new(PLIST_NULL);
    }
}

static PlistNode *decode_object(BplistCtx *ctx, uint64_t index) {
    if (index >= ctx->num_objects) {
        fprintf(stderr, "bplist: object index %llu out of range\n", (unsigned long long)index);
        return plist_new(PLIST_NULL);
    }
    return decode_at(ctx, object_offset(ctx, index));
}

/* Entry point: parse a binary plist held as a byte buffer in memory.
   Returns the root PlistNode, or NULL on failure (bad magic / truncated). */
PlistNode *plist_parse_binary(const char *buf, size_t len) {
    if (len < 40 || memcmp(buf, "bplist00", 8) != 0) {
        fprintf(stderr, "bplist: not a binary plist (bad magic or too short)\n");
        return NULL;
    }

    const char *trailer = buf + len - 32;
    BplistCtx ctx = {
        .buf = buf,
        .len = len,
        .offset_size      = trailer[6],
        .object_ref_size  = trailer[7],
        .num_objects      = read_be_uint(trailer + 8, 8),
        .top_object       = read_be_uint(trailer + 16, 8),
        .offset_table_offset = read_be_uint(trailer + 24, 8),
    };
    ctx.offset_table = buf + ctx.offset_table_offset;

    return decode_object(&ctx, ctx.top_object);
}

// Utility -- give it a string of bytes and an indent depth
// Warning: not proof against malformed data!

int pretty_print_binary_plist(const char *buf, size_t size, int depth) {
  PlistNode *root = plist_parse_binary(buf, (size_t)size);
  if (root) {
      indent(depth);
      plist_print(root, depth);
      plist_free(root);
  }
  return root ? EXIT_SUCCESS : EXIT_FAILURE;
}