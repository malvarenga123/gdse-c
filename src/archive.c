#include "gdse.h"

#include <stdlib.h>
#include <string.h>

#include "../vendor/lz4/lz4.h"

struct gd_arz {
    FILE *file;
    gd_u32 record_count;
    gd_u32 records_offset;
    gd_u32 *record_offsets;
    char **strings;
    gd_u32 string_count;
};

struct gd_arc {
    FILE *file;
    gd_u32 record_count;
    gd_u32 block_list_offset;
    gd_u32 block_list_len;
    gd_u32 record_list_len;
};

static int read_bytes(FILE *file, void *data, size_t length, gd_error *err)
{
    if (fread(data, 1, length, file) != length) {
        gd_set_error(err, "unexpected end of file");
        return 0;
    }
    return 1;
}

static char *read_string(FILE *file, gd_u32 length, gd_error *err)
{
    char *text;
    if (length > 64UL * 1024UL * 1024UL) {
        gd_set_error(err, "unreasonable string length in input");
        return NULL;
    }
    text = (char *)gd_alloc((size_t)length + 1, err);
    if (text == NULL) return NULL;
    if (!read_bytes(file, text, (size_t)length, err)) {
        free(text);
        return NULL;
    }
    text[length] = '\0';
    return text;
}

static void free_strings(gd_arz *db)
{
    gd_u32 i;
    if (db->strings == NULL) return;
    for (i = 0; i < db->string_count; ++i) free(db->strings[i]);
    free(db->strings);
}

gd_arz *gd_arz_open(const char *path, gd_error *err)
{
    gd_arz *db;
    gd_u16 magic;
    gd_u16 version;
    gd_u32 records_len;
    gd_u32 strings_offset;
    gd_u32 strings_len;
    gd_u32 end;
    db = (gd_arz *)gd_alloc(sizeof(*db), err);
    if (db == NULL) return NULL;
    memset(db, 0, sizeof(*db));
    db->file = fopen(path, "rb");
    if (db->file == NULL) {
        gd_set_error(err, "could not open database %s", path);
        free(db);
        return NULL;
    }
    if (!gd_read_u16(db->file, &magic, err) ||
        !gd_read_u16(db->file, &version, err) || magic != 2 || version != 3 ||
        !gd_read_u32(db->file, &db->records_offset, err) ||
        !gd_read_u32(db->file, &records_len, err) ||
        !gd_read_u32(db->file, &db->record_count, err) ||
        !gd_read_u32(db->file, &strings_offset, err) ||
        !gd_read_u32(db->file, &strings_len, err)) {
        if (magic != 2 || version != 3)
            gd_set_error(err, "unsupported ARZ header in %s", path);
        gd_arz_close(db);
        return NULL;
    }
    if (!gd_seek(db->file, strings_offset, err)) {
        gd_arz_close(db);
        return NULL;
    }
    db->record_offsets = (gd_u32 *)gd_alloc((size_t)db->record_count * sizeof(gd_u32), err);
    if (db->record_offsets == NULL || !gd_seek(db->file, db->records_offset, err)) {
        gd_arz_close(db);
        return NULL;
    }
    {
        gd_u32 record_index;
        for (record_index = 0; record_index < db->record_count; ++record_index) {
            gd_u32 kind_length;
            gd_u32 position;
            if (!gd_tell(db->file, &position, err) ||
                !gd_skip(db->file, 4L, err) ||
                !gd_read_u32(db->file, &kind_length, err) || kind_length > 1024UL ||
                !gd_skip(db->file, (long)kind_length + 20L, err)) {
                gd_set_error(err, "invalid ARZ record list in %s", path);
                gd_arz_close(db);
                return NULL;
            }
            db->record_offsets[record_index] = position;
        }
    }
    if (!gd_seek(db->file, strings_offset, err)) {
        gd_arz_close(db);
        return NULL;
    }
    end = strings_offset + strings_len;
    for (;;) {
        gd_u32 count;
        gd_u32 i;
        gd_u32 position;
        char **grown;
        if (!gd_tell(db->file, &position, err)) {
            gd_arz_close(db);
            return NULL;
        }
        if (position >= end) break;
        if (!gd_read_u32(db->file, &count, err) ||
            count > 10000000UL - db->string_count) {
            gd_set_error(err, "invalid ARZ string table in %s", path);
            gd_arz_close(db);
            return NULL;
        }
        grown = (char **)realloc(db->strings,
                  (size_t)(db->string_count + count) * sizeof(char *));
        if (grown == NULL && count != 0) {
            gd_set_error(err, "out of memory");
            gd_arz_close(db);
            return NULL;
        }
        db->strings = grown;
        for (i = 0; i < count; ++i) {
            gd_u32 len;
            if (!gd_read_u32(db->file, &len, err)) {
                gd_arz_close(db);
                return NULL;
            }
            db->strings[db->string_count] = read_string(db->file, len, err);
            if (db->strings[db->string_count] == NULL) {
                gd_arz_close(db);
                return NULL;
            }
            ++db->string_count;
        }
    }
    return db;
}

void gd_arz_close(gd_arz *db)
{
    if (db == NULL) return;
    if (db->file != NULL) fclose(db->file);
    free_strings(db);
    free(db->record_offsets);
    free(db);
}

gd_u32 gd_arz_count(const gd_arz *db) { return db->record_count; }

static gd_u16 mem_u16(const gd_u8 *p)
{
    return (gd_u16)((gd_u16)p[0] | ((gd_u16)p[1] << 8));
}

static gd_u32 mem_u32(const gd_u8 *p)
{
    return (gd_u32)p[0] | ((gd_u32)p[1] << 8) |
           ((gd_u32)p[2] << 16) | ((gd_u32)p[3] << 24);
}

static int parse_fields(gd_arz *db, const gd_u8 *data, size_t length,
                        gd_record *record, gd_error *err)
{
    size_t pos = 0;
    gd_field **tail = &record->fields;
    while (pos < length) {
        gd_u16 kind;
        gd_u16 count;
        gd_u32 key;
        size_t bytes;
        if (length - pos < 8) goto invalid;
        kind = mem_u16(data + pos);
        count = mem_u16(data + pos + 2);
        key = mem_u32(data + pos + 4);
        pos += 8;
        bytes = (size_t)count * 4;
        if (kind > 3 || key >= db->string_count || bytes > length - pos)
            goto invalid;
        if (kind == 2 && count == 1) {
            gd_u32 value = mem_u32(data + pos);
            gd_field *field;
            if (value >= db->string_count) goto invalid;
            field = (gd_field *)gd_alloc(sizeof(*field), err);
            if (field == NULL) return 0;
            field->key = gd_strdup(db->strings[key], err);
            field->value = gd_strdup(db->strings[value], err);
            field->next = NULL;
            if (field->key == NULL || field->value == NULL) {
                free(field->key); free(field->value); free(field);
                return 0;
            }
            *tail = field;
            tail = &field->next;
        }
        pos += bytes;
    }
    return 1;
invalid:
    gd_set_error(err, "invalid ARZ record payload");
    return 0;
}

int gd_arz_record(gd_arz *db, gd_u32 index, gd_record *record, gd_error *err)
{
    gd_u32 string_index;
    gd_u32 kind_len;
    gd_u32 offset;
    gd_u32 compressed_len;
    gd_u32 uncompressed_len;
    gd_u8 *compressed;
    gd_u8 *plain;
    long decoded;
    memset(record, 0, sizeof(*record));
    if (index >= db->record_count ||
        !gd_seek(db->file, db->record_offsets[index], err)) return 0;
    if (!gd_read_u32(db->file, &string_index, err) ||
        !gd_read_u32(db->file, &kind_len, err)) return 0;
    if (string_index >= db->string_count || kind_len > 1024UL ||
        !gd_skip(db->file, (long)kind_len, err) ||
        !gd_read_u32(db->file, &offset, err) ||
        !gd_read_u32(db->file, &compressed_len, err) ||
        !gd_read_u32(db->file, &uncompressed_len, err)) {
        gd_set_error(err, "invalid ARZ record metadata");
        return 0;
    }
    record->id = gd_strdup(db->strings[string_index], err);
    if (record->id == NULL) return 0;
    if (strncmp(record->id, "records/items/", 14) != 0) return 1;
    if (compressed_len > 256UL * 1024UL * 1024UL ||
        uncompressed_len > 256UL * 1024UL * 1024UL ||
        !gd_seek(db->file, offset + 24UL, err)) goto fail;
    compressed = (gd_u8 *)gd_alloc((size_t)compressed_len, err);
    plain = (gd_u8 *)gd_alloc((size_t)uncompressed_len, err);
    if (compressed == NULL || plain == NULL) {
        free(compressed); free(plain); goto fail;
    }
    if (!read_bytes(db->file, compressed, (size_t)compressed_len, err)) {
        free(compressed); free(plain); goto fail;
    }
    decoded = (long)LZ4_decompress_safe((const char *)compressed, (char *)plain,
                  (int)compressed_len, (int)uncompressed_len);
    free(compressed);
    if (decoded != (long)uncompressed_len ||
        !parse_fields(db, plain, (size_t)uncompressed_len, record, err)) {
        if (decoded != (long)uncompressed_len)
            gd_set_error(err, "could not decompress ARZ record %s", record->id);
        free(plain); goto fail;
    }
    free(plain);
    return 1;
fail:
    gd_record_free(record);
    return 0;
}

void gd_record_free(gd_record *record)
{
    gd_field *field = record->fields;
    while (field != NULL) {
        gd_field *next = field->next;
        free(field->key); free(field->value); free(field);
        field = next;
    }
    free(record->id);
    memset(record, 0, sizeof(*record));
}

const char *gd_record_field(const gd_record *record, const char *key)
{
    gd_field *field;
    for (field = record->fields; field != NULL; field = field->next)
        if (strcmp(field->key, key) == 0) return field->value;
    return NULL;
}

gd_arc *gd_arc_open(const char *path, gd_error *err)
{
    gd_arc *arc = (gd_arc *)gd_alloc(sizeof(*arc), err);
    gd_u32 magic, version, block_count;
    if (arc == NULL) return NULL;
    memset(arc, 0, sizeof(*arc));
    arc->file = fopen(path, "rb");
    if (arc->file == NULL || !gd_read_u32(arc->file, &magic, err) ||
        !gd_read_u32(arc->file, &version, err) || magic != 4411969UL ||
        version != 3 || !gd_read_u32(arc->file, &arc->record_count, err) ||
        !gd_read_u32(arc->file, &block_count, err) ||
        !gd_read_u32(arc->file, &arc->block_list_len, err) ||
        !gd_read_u32(arc->file, &arc->record_list_len, err) ||
        !gd_read_u32(arc->file, &arc->block_list_offset, err)) {
        gd_set_error(err, "could not read supported ARC %s", path);
        gd_arc_close(arc);
        return NULL;
    }
    return arc;
}

void gd_arc_close(gd_arc *arc)
{
    if (arc == NULL) return;
    if (arc->file != NULL) fclose(arc->file);
    free(arc);
}

gd_u32 gd_arc_count(const gd_arc *arc) { return arc->record_count; }

static char *arc_name(gd_arc *arc, gd_u32 index, gd_error *err)
{
    gd_u32 i;
    size_t cap = 64, len = 0;
    char *name;
    if (!gd_seek(arc->file, arc->block_list_offset + arc->block_list_len, err))
        return NULL;
    name = (char *)gd_alloc(cap, err);
    if (name == NULL) return NULL;
    for (i = 0; i <= index; ++i) {
        int c;
        len = 0;
        while ((c = fgetc(arc->file)) != EOF && c != 0) {
            if (len + 1 >= cap) {
                char *grown;
                cap *= 2;
                grown = (char *)realloc(name, cap);
                if (grown == NULL) { free(name); gd_set_error(err, "out of memory"); return NULL; }
                name = grown;
            }
            name[len++] = (char)c;
        }
        if (c == EOF) { free(name); gd_set_error(err, "invalid ARC name list"); return NULL; }
    }
    name[len] = '\0';
    return name;
}

int gd_arc_record(gd_arc *arc, gd_u32 index, char **id, gd_u8 **data,
                  size_t *length, gd_error *err)
{
    gd_u32 metadata_offset = arc->block_list_offset + arc->block_list_len +
                             arc->record_list_len + index * 44UL;
    gd_u32 ignored, uncompressed_len, block_count, block_index, i;
    size_t out = 0;
    *id = NULL; *data = NULL; *length = 0;
    if (index >= arc->record_count || !gd_seek(arc->file, metadata_offset, err)) return 0;
    if (!gd_read_u32(arc->file, &ignored, err) ||
        !gd_read_u32(arc->file, &ignored, err) ||
        !gd_read_u32(arc->file, &ignored, err) ||
        !gd_read_u32(arc->file, &uncompressed_len, err) ||
        !gd_skip(arc->file, 12L, err) ||
        !gd_read_u32(arc->file, &block_count, err) ||
        !gd_read_u32(arc->file, &block_index, err)) return 0;
    *id = arc_name(arc, index, err);
    *data = (gd_u8 *)gd_alloc((size_t)uncompressed_len + 1, err);
    if (*id == NULL || *data == NULL) goto fail;
    for (i = 0; i < block_count; ++i) {
        gd_u32 offset, compressed_len, plain_len;
        gd_u8 *compressed;
        if (!gd_seek(arc->file, arc->block_list_offset + (block_index + i) * 12UL, err) ||
            !gd_read_u32(arc->file, &offset, err) ||
            !gd_read_u32(arc->file, &compressed_len, err) ||
            !gd_read_u32(arc->file, &plain_len, err) ||
            out + plain_len > uncompressed_len || !gd_seek(arc->file, offset, err)) goto fail;
        if (compressed_len == plain_len) {
            if (!read_bytes(arc->file, *data + out, (size_t)plain_len, err)) goto fail;
        } else {
            int decoded;
            compressed = (gd_u8 *)gd_alloc((size_t)compressed_len, err);
            if (compressed == NULL || !read_bytes(arc->file, compressed,
                                                  (size_t)compressed_len, err)) {
                free(compressed); goto fail;
            }
            decoded = LZ4_decompress_safe((const char *)compressed,
                       (char *)(*data + out), (int)compressed_len, (int)plain_len);
            free(compressed);
            if (decoded != (int)plain_len) { gd_set_error(err, "invalid ARC block"); goto fail; }
        }
        out += plain_len;
    }
    if (out != uncompressed_len) { gd_set_error(err, "incomplete ARC record"); goto fail; }
    (*data)[out] = 0;
    *length = out;
    return 1;
fail:
    free(*id); free(*data); *id = NULL; *data = NULL;
    return 0;
}
