#ifndef GDSE_H
#define GDSE_H

#include <stddef.h>
#include <stdio.h>

typedef unsigned char gd_u8;
typedef unsigned short gd_u16;
typedef unsigned long gd_u32;

typedef struct gd_error {
    char message[512];
} gd_error;

typedef enum gd_rarity {
    GD_COMMON, GD_MAGICAL, GD_RARE, GD_EPIC, GD_LEGENDARY, GD_UNKNOWN
} gd_rarity;

typedef enum gd_kind { GD_ITEM, GD_AFFIX } gd_kind;

typedef struct gd_tag {
    char *name;
    gd_kind kind;
    gd_rarity rarity;
    int affixable;
    unsigned long counts[5];
    int faction;
    int gear;
    int item_present;
    int set_item;
    int name_part;
    struct gd_tag *next;
    struct gd_tag *hash_next;
} gd_tag;

typedef struct gd_part {
    char *name;
    char *base;
    int affixable;
    struct gd_part *next;
    struct gd_part *hash_next;
} gd_part;

typedef struct gd_inference {
    gd_tag *tags;
    gd_part *parts;
    gd_tag **tag_buckets;
    gd_part **part_buckets;
    size_t tag_bucket_count;
    size_t part_bucket_count;
    size_t tag_count;
    size_t part_count;
} gd_inference;

typedef struct gd_field {
    char *key;
    char *value;
    struct gd_field *next;
} gd_field;

typedef struct gd_record {
    char *id;
    gd_field *fields;
} gd_record;

typedef struct gd_arz gd_arz;
typedef struct gd_arc gd_arc;

void gd_set_error(gd_error *err, const char *fmt, ...);
void *gd_alloc(size_t size, gd_error *err);
char *gd_strdup(const char *text, gd_error *err);
int gd_read_u16(FILE *file, gd_u16 *value, gd_error *err);
int gd_read_u32(FILE *file, gd_u32 *value, gd_error *err);
int gd_seek(FILE *file, gd_u32 offset, gd_error *err);
int gd_skip(FILE *file, long offset, gd_error *err);
int gd_tell(FILE *file, gd_u32 *offset, gd_error *err);
int gd_path_exists(const char *path);
int gd_is_directory(const char *path);
int gd_mkdirs(const char *path, gd_error *err);
char *gd_path_join(const char *left, const char *right, gd_error *err);
int gd_safe_record_path(const char *path);

gd_arz *gd_arz_open(const char *path, gd_error *err);
void gd_arz_close(gd_arz *db);
gd_u32 gd_arz_count(const gd_arz *db);
int gd_arz_record(gd_arz *db, gd_u32 index, gd_record *record, gd_error *err);
void gd_record_free(gd_record *record);
const char *gd_record_field(const gd_record *record, const char *key);

gd_arc *gd_arc_open(const char *path, gd_error *err);
void gd_arc_close(gd_arc *arc);
gd_u32 gd_arc_count(const gd_arc *arc);
int gd_arc_record(gd_arc *arc, gd_u32 index, char **id, gd_u8 **data,
                  size_t *length, gd_error *err);

void gd_inference_init(gd_inference *inference);
gd_tag *gd_inference_ensure_tag(gd_inference *inference, const char *name,
                                gd_error *err);
int gd_inference_add_part(gd_inference *inference, const char *part,
                          const char *base, gd_error *err);
int gd_infer_database(gd_inference *inference, gd_arz *db, gd_error *err);
void gd_inference_finish(gd_inference *inference, gd_error *err);
void gd_inference_free(gd_inference *inference);
const gd_tag *gd_tag_lookup(const gd_inference *inference, const char *name);
char gd_tag_color(const gd_inference *inference, const char *tag,
                  int full_rainbow);
char gd_property_color(const char *tag, int rainbow);
char *gd_apply_color(const char *value, char color, gd_error *err);
char *gd_recolor_text(const char *text, size_t length,
                      const gd_inference *inference, int rainbow,
                      int full_rainbow, unsigned long *colored,
                      size_t *out_length, gd_error *err);

#endif
