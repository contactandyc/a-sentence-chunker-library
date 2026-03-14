// SPDX-FileCopyrightText: 2024-2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-FileCopyrightText: 2024–2025 Knode.ai — technical questions: contact Andy (above)
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "a-json-library/ajson.h"
#include "a-memory-library/aml_pool.h"
#include "a-sentence-chunker-library/a_sentence_chunker.h"

#define MAX_PATH_LEN 1024

// Helper function: read entire file into memory.
char *read_file(const char *filename, size_t *out_length) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);
    char *buffer = malloc(fsize + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    fread(buffer, 1, fsize, fp);
    fclose(fp);
    buffer[fsize] = '\0';
    if (out_length)
        *out_length = fsize;
    return buffer;
}

// Normalize Gutenberg-style line breaks:
// - collapse single '\n' into space
// - preserve double '\n\n' as real paragraph breaks
static char *normalize_newlines(const char *text) {
    size_t len = strlen(text);
    char *out = malloc(len * 2 + 1); // safe upper bound
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            if (i + 1 < len && text[i+1] == '\n') {
                // true paragraph break → keep as double newline
                out[j++] = '\n';
                out[j++] = '\n';
                i++; // skip the second \n
            } else {
                // mid-paragraph line break → space
                out[j++] = ' ';
            }
        } else {
            out[j++] = text[i];
        }
    }

    out[j] = '\0';
    return out;
}

static void process_non_json_file(const char *filename) {
    size_t length = 0;
    char *content = read_file(filename, &length);
    if (!content) {
        fprintf(stderr, "Could not read file: %s\n", filename);
        return;
    }

    // --- NEW: normalize before chunking ---
    char *normalized = normalize_newlines(content);
    free(content);
    if (!normalized) {
        fprintf(stderr, "Memory error while normalizing file: %s\n", filename);
        return;
    }

    // Create buffers for sentence chunking
    aml_buffer_t *bh1 = aml_buffer_init(32);
    aml_buffer_t *bh2 = aml_buffer_init(32);

    // First pass
    size_t num_first_chunks = 0;
    a_sentence_chunk_t *first_chunks = a_sentence_chunker(&num_first_chunks, bh1, normalized);

    // Second pass (enforces min/max length, etc.)
    size_t num_chunks = 0;
    a_sentence_chunk_t *chunks = a_rechunk_sentences(
        &num_chunks,
        bh2,
        normalized,
        first_chunks,
        num_first_chunks,
        5,    // min_length
        1000   // max_length
    );

    // Print each sentence on its own line
    for (size_t i = 0; i < num_chunks; i++) {
        a_sentence_chunk_t *c = &chunks[i];
        size_t off = c->start_offset;
        size_t ln = c->length;
        size_t norm_len = strlen(normalized);

        if (off + ln > norm_len) {
            ln = (off < norm_len) ? (norm_len - off) : 0;
        }

        char *sentence = malloc(ln + 1);
        memcpy(sentence, normalized + off, ln);
        sentence[ln] = '\0';

        printf("%s\n", sentence);
        free(sentence);
    }

    aml_buffer_destroy(bh1);
    aml_buffer_destroy(bh2);
    free(normalized);
}

// ------------------------------------------------------------------
// main: decides how to handle the single path argument.
// ------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    process_non_json_file(filename);

    return 0;
}
