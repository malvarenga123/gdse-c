#include "gdse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Keep the project sources C89-clean while isolating utf8proc's public ABI. */
typedef ptrdiff_t gd_utf8proc_ssize;
typedef int gd_utf8proc_int32;
extern gd_utf8proc_ssize utf8proc_iterate(const unsigned char *,
                                          gd_utf8proc_ssize,
                                          gd_utf8proc_int32 *);
extern int utf8proc_category(gd_utf8proc_int32);
#define GD_UTF8PROC_LU 1
#define GD_UTF8PROC_LO 5

static int starts(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static gd_rarity rarity(const char *text)
{
    if (text == NULL) return GD_UNKNOWN;
    if (strcmp(text, "Common") == 0) return GD_COMMON;
    if (strcmp(text, "Magical") == 0) return GD_MAGICAL;
    if (strcmp(text, "Rare") == 0) return GD_RARE;
    if (strcmp(text, "Epic") == 0) return GD_EPIC;
    if (strcmp(text, "Legendary") == 0) return GD_LEGENDARY;
    return GD_UNKNOWN;
}

static unsigned long hash_text(const char *text)
{
    unsigned long hash = 2166136261UL;
    while (*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= 16777619UL;
    }
    return hash;
}

static unsigned long hash_part(const char *name, const char *base)
{
    unsigned long hash = hash_text(name);
    while (*base != '\0') {
        hash ^= (unsigned char)*base++;
        hash *= 16777619UL;
    }
    return hash;
}

static gd_tag *find_tag(const gd_inference *inference, const char *name)
{
    gd_tag *tag;
    size_t bucket;
    if (inference->tag_bucket_count == 0) return NULL;
    bucket = (size_t)(hash_text(name) % inference->tag_bucket_count);
    for (tag = inference->tag_buckets[bucket]; tag != NULL;
         tag = tag->hash_next)
        if (strcmp(tag->name, name) == 0) return tag;
    return NULL;
}

static int resize_tags(gd_inference *inference, size_t count, gd_error *err)
{
    gd_tag **buckets;
    gd_tag *tag;
    size_t i;
    if (count > ((size_t)-1) / sizeof(*buckets)) {
        gd_set_error(err, "tag index is too large");
        return 0;
    }
    buckets = (gd_tag **)gd_alloc(count * sizeof(*buckets), err);
    if (buckets == NULL) return 0;
    memset(buckets, 0, count * sizeof(*buckets));
    for (tag = inference->tags; tag != NULL; tag = tag->next) {
        i = (size_t)(hash_text(tag->name) % count);
        tag->hash_next = buckets[i];
        buckets[i] = tag;
    }
    free(inference->tag_buckets);
    inference->tag_buckets = buckets;
    inference->tag_bucket_count = count;
    return 1;
}

gd_tag *gd_inference_ensure_tag(gd_inference *inference, const char *name,
                                gd_error *err)
{
    gd_tag *tag = find_tag(inference, name);
    size_t bucket;
    if (tag != NULL) return tag;
    if (inference->tag_bucket_count == 0) {
        if (!resize_tags(inference, 16, err)) return NULL;
    } else if (inference->tag_count >=
               inference->tag_bucket_count - inference->tag_bucket_count / 4) {
        if (inference->tag_bucket_count > ((size_t)-1) / 2 ||
            !resize_tags(inference, inference->tag_bucket_count * 2, err))
            return NULL;
    }
    tag = (gd_tag *)gd_alloc(sizeof(*tag), err);
    if (tag == NULL) return NULL;
    memset(tag, 0, sizeof(*tag));
    tag->name = gd_strdup(name, err);
    if (tag->name == NULL) { free(tag); return NULL; }
    tag->rarity = GD_UNKNOWN;
    tag->next = inference->tags;
    inference->tags = tag;
    bucket = (size_t)(hash_text(name) % inference->tag_bucket_count);
    tag->hash_next = inference->tag_buckets[bucket];
    inference->tag_buckets[bucket] = tag;
    ++inference->tag_count;
    return tag;
}

void gd_inference_init(gd_inference *inference)
{
    inference->tags = NULL;
    inference->parts = NULL;
    inference->item_paths = NULL;
    inference->loot_tables = NULL;
    inference->tag_buckets = NULL;
    inference->part_buckets = NULL;
    inference->item_path_buckets = NULL;
    inference->loot_table_buckets = NULL;
    inference->tag_bucket_count = 0;
    inference->part_bucket_count = 0;
    inference->item_path_bucket_count = 0;
    inference->loot_table_bucket_count = 0;
    inference->tag_count = 0;
    inference->part_count = 0;
    inference->item_path_count = 0;
    inference->loot_table_count = 0;
    inference->creature_serial = 0;
}

static int resize_parts(gd_inference *inference, size_t count, gd_error *err)
{
    gd_part **buckets;
    gd_part *part;
    size_t i;
    if (count > ((size_t)-1) / sizeof(*buckets)) {
        gd_set_error(err, "part index is too large");
        return 0;
    }
    buckets = (gd_part **)gd_alloc(count * sizeof(*buckets), err);
    if (buckets == NULL) return 0;
    memset(buckets, 0, count * sizeof(*buckets));
    for (part = inference->parts; part != NULL; part = part->next) {
        i = (size_t)(hash_part(part->name, part->base) % count);
        part->hash_next = buckets[i];
        buckets[i] = part;
    }
    free(inference->part_buckets);
    inference->part_buckets = buckets;
    inference->part_bucket_count = count;
    return 1;
}

int gd_inference_add_part(gd_inference *inference, const char *part,
                          const char *base, gd_error *err)
{
    gd_part *item;
    size_t bucket;
    if (part == NULL || *part == '\0') return 1;
    if (inference->part_bucket_count == 0) {
        if (!resize_parts(inference, 16, err)) return 0;
    }
    bucket = (size_t)(hash_part(part, base) % inference->part_bucket_count);
    for (item = inference->part_buckets[bucket]; item != NULL;
         item = item->hash_next)
        if (strcmp(item->name, part) == 0 && strcmp(item->base, base) == 0)
            return 1;
    if (inference->part_count >=
        inference->part_bucket_count - inference->part_bucket_count / 4) {
        if (inference->part_bucket_count > ((size_t)-1) / 2 ||
            !resize_parts(inference, inference->part_bucket_count * 2, err))
            return 0;
        bucket = (size_t)(hash_part(part, base) % inference->part_bucket_count);
    }
    item = (gd_part *)gd_alloc(sizeof(*item), err);
    if (item == NULL) return 0;
    item->name = gd_strdup(part, err);
    item->base = gd_strdup(base, err);
    if (item->name == NULL || item->base == NULL) {
        free(item->name); free(item->base); free(item); return 0;
    }
    item->next = inference->parts;
    inference->parts = item;
    item->hash_next = inference->part_buckets[bucket];
    inference->part_buckets[bucket] = item;
    ++inference->part_count;
    return 1;
}

/* Loot tables name their contents by record path, and a monster names its drop
   tables the same way, so Monster Infrequent inference needs two more indexes:
   record path -> itemNameTag, and loot-table path -> the paths it contains. */

static gd_item_path *find_item_path(const gd_inference *inference,
                                    const char *path)
{
    gd_item_path *entry;
    size_t bucket;
    if (inference->item_path_bucket_count == 0) return NULL;
    bucket = (size_t)(hash_text(path) % inference->item_path_bucket_count);
    for (entry = inference->item_path_buckets[bucket]; entry != NULL;
         entry = entry->hash_next)
        if (strcmp(entry->path, path) == 0) return entry;
    return NULL;
}

static int resize_item_paths(gd_inference *inference, size_t count,
                             gd_error *err)
{
    gd_item_path **buckets;
    gd_item_path *entry;
    size_t i;
    if (count > ((size_t)-1) / sizeof(*buckets)) {
        gd_set_error(err, "item path index is too large");
        return 0;
    }
    buckets = (gd_item_path **)gd_alloc(count * sizeof(*buckets), err);
    if (buckets == NULL) return 0;
    memset(buckets, 0, count * sizeof(*buckets));
    for (entry = inference->item_paths; entry != NULL; entry = entry->next) {
        i = (size_t)(hash_text(entry->path) % count);
        entry->hash_next = buckets[i];
        buckets[i] = entry;
    }
    free(inference->item_path_buckets);
    inference->item_path_buckets = buckets;
    inference->item_path_bucket_count = count;
    return 1;
}

static int add_item_path(gd_inference *inference, const char *path,
                         const char *tag, gd_error *err)
{
    gd_item_path *entry;
    size_t bucket;
    if (find_item_path(inference, path) != NULL) return 1;
    if (inference->item_path_bucket_count == 0) {
        if (!resize_item_paths(inference, 16, err)) return 0;
    } else if (inference->item_path_count >=
               inference->item_path_bucket_count -
               inference->item_path_bucket_count / 4) {
        if (inference->item_path_bucket_count > ((size_t)-1) / 2 ||
            !resize_item_paths(inference,
                               inference->item_path_bucket_count * 2, err))
            return 0;
    }
    entry = (gd_item_path *)gd_alloc(sizeof(*entry), err);
    if (entry == NULL) return 0;
    memset(entry, 0, sizeof(*entry));
    entry->path = gd_strdup(path, err);
    entry->tag = gd_strdup(tag, err);
    if (entry->path == NULL || entry->tag == NULL) {
        free(entry->path); free(entry->tag); free(entry); return 0;
    }
    entry->next = inference->item_paths;
    inference->item_paths = entry;
    bucket = (size_t)(hash_text(path) % inference->item_path_bucket_count);
    entry->hash_next = inference->item_path_buckets[bucket];
    inference->item_path_buckets[bucket] = entry;
    ++inference->item_path_count;
    return 1;
}

static gd_loot_table *find_loot_table(const gd_inference *inference,
                                      const char *path)
{
    gd_loot_table *table;
    size_t bucket;
    if (inference->loot_table_bucket_count == 0) return NULL;
    bucket = (size_t)(hash_text(path) % inference->loot_table_bucket_count);
    for (table = inference->loot_table_buckets[bucket]; table != NULL;
         table = table->hash_next)
        if (strcmp(table->path, path) == 0) return table;
    return NULL;
}

static int resize_loot_tables(gd_inference *inference, size_t count,
                              gd_error *err)
{
    gd_loot_table **buckets;
    gd_loot_table *table;
    size_t i;
    if (count > ((size_t)-1) / sizeof(*buckets)) {
        gd_set_error(err, "loot table index is too large");
        return 0;
    }
    buckets = (gd_loot_table **)gd_alloc(count * sizeof(*buckets), err);
    if (buckets == NULL) return 0;
    memset(buckets, 0, count * sizeof(*buckets));
    for (table = inference->loot_tables; table != NULL; table = table->next) {
        i = (size_t)(hash_text(table->path) % count);
        table->hash_next = buckets[i];
        buckets[i] = table;
    }
    free(inference->loot_table_buckets);
    inference->loot_table_buckets = buckets;
    inference->loot_table_bucket_count = count;
    return 1;
}

/* A creature may name a table before or after the table's own record is read,
   so the entry is created on first mention either way. */
static gd_loot_table *ensure_loot_table(gd_inference *inference,
                                        const char *path, gd_error *err)
{
    gd_loot_table *table = find_loot_table(inference, path);
    size_t bucket;
    if (table != NULL) return table;
    if (inference->loot_table_bucket_count == 0) {
        if (!resize_loot_tables(inference, 16, err)) return NULL;
    } else if (inference->loot_table_count >=
               inference->loot_table_bucket_count -
               inference->loot_table_bucket_count / 4) {
        if (inference->loot_table_bucket_count > ((size_t)-1) / 2 ||
            !resize_loot_tables(inference,
                                inference->loot_table_bucket_count * 2, err))
            return NULL;
    }
    table = (gd_loot_table *)gd_alloc(sizeof(*table), err);
    if (table == NULL) return NULL;
    memset(table, 0, sizeof(*table));
    table->path = gd_strdup(path, err);
    if (table->path == NULL) { free(table); return NULL; }
    table->next = inference->loot_tables;
    inference->loot_tables = table;
    bucket = (size_t)(hash_text(path) % inference->loot_table_bucket_count);
    table->hash_next = inference->loot_table_buckets[bucket];
    inference->loot_table_buckets[bucket] = table;
    ++inference->loot_table_count;
    return table;
}

/* A world-drop pool: filed in the shared mastertables layer, and not one of the
   few records there that a single boss claims for itself. A mastertable no
   creature names directly counts as shared, so nesting cannot wander into the
   pooling layer sideways. */
static int is_shared_pool(const gd_loot_table *table)
{
    if (strstr(table->path, "/loottables/mastertables/") == NULL) return 0;
    return !(table->creature_refs >= 1 &&
             table->creature_refs <= GD_SHARED_TABLE_REFS);
}

/* A specialized loot-table path carries a discriminator after its rarity
   token (d02_alkamos, c03_sharzul, and so on), or names the shared ghostly
   family. Generic tier wrappers end at the token (c01, d101) and expand into
   every item of that tier. */
static int is_specialized_loot_path(const char *path)
{
    const char *name = strrchr(path, '/');
    const char *p;
    name = name == NULL ? path : name + 1;
    if (strstr(name, "_ghostly.dbr") != NULL) return 1;
    for (p = name; *p != '\0'; ++p) {
        const char *q;
        if (*p != '_' || (p[1] < 'a' || p[1] > 'd') ||
            p[2] < '0' || p[2] > '9') continue;
        q = p + 2;
        while (*q >= '0' && *q <= '9') ++q;
        if (*q >= 'a' && *q <= 'z') ++q;
        if (*q == '_' && q[1] != '\0' && strcmp(q + 1, "dbr") != 0)
            return 1;
    }
    return 0;
}

static int add_loot_entry_len(gd_inference *inference, const char *table_path,
                              const char *item_path, size_t len, gd_error *err)
{
    gd_loot_table *table = ensure_loot_table(inference, table_path, err);
    gd_loot_entry *entry;
    if (table == NULL) return 0;
    entry = (gd_loot_entry *)gd_alloc(sizeof(*entry), err);
    if (entry == NULL) return 0;
    entry->item_path = (char *)gd_alloc(len + 1, err);
    if (entry->item_path == NULL) { free(entry); return 0; }
    memcpy(entry->item_path, item_path, len);
    entry->item_path[len] = '\0';
    entry->next = table->entries;
    table->entries = entry;
    return 1;
}

static int add_loot_entry(gd_inference *inference, const char *table_path,
                          const char *item_path, gd_error *err)
{
    return add_loot_entry_len(inference, table_path, item_path,
                              strlen(item_path), err);
}

/* lootName1..N name the records a table can yield. A LevelTable instead lists
   its child tables in a `records` string array. Follow that array only when
   both paths are specialized: this reaches named boss/monster families while
   excluding generic tier children such as tdyn_head_c01, which otherwise turn
   whole Epic and Legendary pools into false Monster Infrequents. */
static int scan_loot_table(gd_inference *inference, const gd_record *record,
                           gd_error *err)
{
    const gd_field *field;
    for (field = record->fields; field != NULL; field = field->next) {
        if (!starts(field->key, "lootName") &&
            !(strcmp(field->key, "records") == 0 &&
              is_specialized_loot_path(record->id) &&
              is_specialized_loot_path(field->value))) continue;
        if (*field->value == '\0') continue;
        if (!add_loot_entry(inference, record->id, field->value, err)) return 0;
    }
    return 1;
}

/* A monster names its loot in loot<Slot>Item<M> fields, covering both the misc
   drop slots and the equipment slots -- which slot an item uses says nothing
   about what it is. A yeti carries its Monster Infrequent in lootMisc3Item1,
   while the troll that drops Gollus' Ring carries it in lootFinger1Item1 and
   its misc slots hold only pools shared with the rest of the world.
   Count how many distinct creatures name each table rather than deciding from
   its path. Pets and other non-monster actors never reach here: they are Class
   Pet and live outside records/creatures/. */
static int scan_creature(gd_inference *inference, const gd_record *record,
                         gd_error *err)
{
    const gd_field *field;
    const char *class_name = gd_record_field(record, "Class");
    unsigned long serial;
    if (class_name == NULL || !starts(class_name, "Monster")) return 1;
    serial = ++inference->creature_serial;
    for (field = record->fields; field != NULL; field = field->next) {
        gd_loot_table *table;
        if (!starts(field->key, "loot") ||
            strstr(field->key, "Item") == NULL) continue;
        if (*field->value == '\0') continue;
        table = ensure_loot_table(inference, field->value, err);
        if (table == NULL) return 0;
        /* One creature naming the same table in six equipment slots is still
           one creature. */
        if (table->ref_serial != serial) {
            table->ref_serial = serial;
            ++table->creature_refs;
        }
    }
    return 1;
}

/* Thin wrappers so the test suite can build inference state directly; the
   loot-table indexes are otherwise only reachable from a real database. */
int add_item_path_for_test(gd_inference *inference, const char *path,
                           const char *tag, gd_error *err)
{
    return add_item_path(inference, path, tag, err);
}

int add_loot_entry_for_test(gd_inference *inference, const char *table_path,
                            const char *item_path, gd_error *err)
{
    return add_loot_entry(inference, table_path, item_path, err);
}

gd_loot_table *ensure_loot_table_for_test(gd_inference *inference,
                                          const char *path, gd_error *err)
{
    return ensure_loot_table(inference, path, err);
}

gd_loot_table *find_loot_table_for_test(const gd_inference *inference,
                                        const char *path)
{
    return find_loot_table(inference, path);
}

int scan_loot_table_for_test(gd_inference *inference, const gd_record *record,
                             gd_error *err)
{
    return scan_loot_table(inference, record, err);
}

int gd_infer_database(gd_inference *inference, gd_arz *db, gd_error *err)
{
    gd_u32 i;
    for (i = 0; i < gd_arz_count(db); ++i) {
        gd_record record;
        const char *tag_name;
        const char *rarity_text;
        gd_rarity rank;
        gd_tag *tag;
        if (!gd_arz_record(db, i, &record, err)) return 0;
        if (starts(record.id, "records/creatures/")) {
            if (!scan_creature(inference, &record, err)) { gd_record_free(&record); return 0; }
            gd_record_free(&record); continue;
        }
        if (!starts(record.id, "records/items/")) { gd_record_free(&record); continue; }
        if (starts(record.id, "records/items/loottables/")) {
            if (!scan_loot_table(inference, &record, err)) { gd_record_free(&record); return 0; }
            gd_record_free(&record); continue;
        }
        if (starts(record.id, "records/items/lootaffixes/prefix/") ||
            starts(record.id, "records/items/lootaffixes/suffix/")) {
            tag_name = gd_record_field(&record, "lootRandomizerName");
            rank = rarity(gd_record_field(&record, "itemClassification"));
            if (tag_name != NULL && *tag_name != '\0' && rank != GD_UNKNOWN) {
                tag = gd_inference_ensure_tag(inference, tag_name, err);
                if (tag == NULL) { gd_record_free(&record); return 0; }
                if (!tag->item_present) {
                    tag->kind = GD_AFFIX;
                    tag->rarity = rank;
                }
            }
        } else {
            const char *class_name;
            const char *part;
            const char *set_name;
            tag_name = gd_record_field(&record, "itemNameTag");
            if (tag_name == NULL || *tag_name == '\0') { gd_record_free(&record); continue; }
            tag = gd_inference_ensure_tag(inference, tag_name, err);
            if (tag == NULL) { gd_record_free(&record); return 0; }
            if (!add_item_path(inference, record.id, tag_name, err)) {
                gd_record_free(&record); return 0;
            }
            /* A non-empty itemSetName points at the set record this base
               belongs to; Full Rainbow marks those names with "(S) ". One tag
               is often shared by a base item and its Empowered/Mythical
               upgrades, and those tiers do not all belong to the same set, so
               tally the records and resolve by majority in the same way rarity
               does rather than latching on the first set-bearing record. */
            ++tag->name_records;
            set_name = gd_record_field(&record, "itemSetName");
            if (set_name != NULL && *set_name != '\0') ++tag->set_records;
            class_name = gd_record_field(&record, "Class");
            if (class_name != NULL && (starts(class_name, "Weapon") ||
                                       starts(class_name, "Armor"))) tag->gear = 1;
            part = gd_record_field(&record, "itemStyleTag");
            if (!gd_inference_add_part(inference, part, tag_name, err)) { gd_record_free(&record); return 0; }
            part = gd_record_field(&record, "itemQualityTag");
            if (!gd_inference_add_part(inference, part, tag_name, err)) { gd_record_free(&record); return 0; }
            rarity_text = gd_record_field(&record, "itemClassification");
            rank = rarity(rarity_text);
            if (rank != GD_UNKNOWN) {
                tag->kind = GD_ITEM;
                tag->item_present = 1;
                if (starts(record.id, "records/items/faction/")) tag->faction = 1;
                ++tag->counts[(int)rank];
            } else if (rarity_text != NULL &&
                       strcmp(rarity_text, "Broken") == 0) {
                tag->kind = GD_ITEM;
                tag->item_present = 1;
                tag->broken_item = 1;
            }
        }
        gd_record_free(&record);
    }
    return 1;
}

void gd_inference_finish(gd_inference *inference, gd_error *err)
{
    gd_tag *tag;
    gd_part *part;
    gd_loot_table *table;
    (void)err;
    for (tag = inference->tags; tag != NULL; tag = tag->next) {
        int i, best = -1;
        unsigned long count = 0;
        if (!tag->item_present) continue;
        for (i = 0; i < 5; ++i) {
            if (tag->counts[i] > count) { count = tag->counts[i]; best = i; }
        }
        tag->rarity = best < 0 ? GD_UNKNOWN : (gd_rarity)best;
        tag->affixable = tag->gear && !tag->faction && best >= 0 && best <= GD_RARE;
        tag->set_item = tag->set_records * 2 > tag->name_records;
    }
    /* mastertables/ is the shared pooling layer, and a table a creature names
       outside it belongs to that creature. The reference count is not a general
       replacement for that structure: a monster family has one creature record
       per variant and difficulty, so dozens of yetis name the one yeti table,
       and judging by count alone throws it away.
       Within mastertables/ the count does separate the exceptions. A few of
       those records are one boss's own table rather than a world pool, and they
       are not distinguishable by name or family --
       mt_accessories_rings_d02_alkamos sits beside mt_accessories_rings_d01, at
       1 creature against 50. */
    for (table = inference->loot_tables; table != NULL; table = table->next) {
        gd_loot_entry *entry;
        if (table->creature_refs == 0) continue;
        /* Even a widely shared monster-equipment mastertable can own a named
           family such as the ghostly weapons. Seed only its specialized child;
           never expand the shared table itself. */
        if (is_shared_pool(table)) {
            for (entry = table->entries; entry != NULL; entry = entry->next) {
                gd_loot_table *nested;
                if (!is_specialized_loot_path(entry->item_path)) continue;
                nested = find_loot_table(inference, entry->item_path);
                if (nested == NULL) continue;
                nested->monster_drop = 1;
                nested->expandable = 1;
            }
            continue;
        }
        table->monster_drop = 1;
        /* Only a boss's own mastertable is followed onward. A creature also
           names generic tables for the gear it wields -- a LevelTable covering
           every Epic sword of its class, say -- and those expand into the whole
           high-tier pool. Their direct contents still count, which is what
           reaches an Infrequent a monster names outright. */
        if (strstr(table->path, "/loottables/mastertables/") != NULL)
            table->expandable = 1;
    }
    /* A boss's mastertable is a wrapper: its entries are further tables, and
       the items behind them are its Infrequents just the same. Alkamos reaches
       Soulrend through mt_gearweaponsmelee2h_d02_alkamos ->
       lt_melee2h_d02_alkamos -> tdyn_melee2h_d02_alkamos.
       Spread the mark to a fixed point, following only what a wrapper leads to
       and stopping at any table shared widely enough to be a world pool. */
    for (;;) {
        int changed = 0;
        for (table = inference->loot_tables; table != NULL; table = table->next) {
            gd_loot_entry *entry;
            if (!table->expandable) continue;
            for (entry = table->entries; entry != NULL; entry = entry->next) {
                gd_loot_table *nested =
                    find_loot_table(inference, entry->item_path);
                if (nested == NULL || nested->expandable) continue;
                if (is_specialized_loot_path(table->path) &&
                    !is_specialized_loot_path(nested->path)) continue;
                if (is_shared_pool(nested)) continue;
                nested->monster_drop = 1;
                nested->expandable = 1;
                changed = 1;
            }
        }
        if (!changed) break;
    }
    /* Every item in a table a monster names in its own drop slots is a Monster
       Infrequent. The table may be read before or after the creature naming it,
       so this resolves once both passes are complete. */
    for (table = inference->loot_tables; table != NULL; table = table->next) {
        gd_loot_entry *entry;
        if (!table->monster_drop) continue;
        for (entry = table->entries; entry != NULL; entry = entry->next) {
            const gd_item_path *item =
                find_item_path(inference, entry->item_path);
            gd_tag *owner;
            if (item == NULL) continue;
            owner = find_tag(inference, item->tag);
            /* Only Rare and above. A monster's loot slots also hold the
               ordinary Common gear it wields, which is not an Infrequent and
               which Full Rainbow leaves at its base color. */
            if (owner != NULL &&
                (owner->rarity == GD_RARE || owner->rarity == GD_EPIC ||
                 owner->rarity == GD_LEGENDARY))
                owner->monster_infrequent = 1;
        }
    }
    for (part = inference->parts; part != NULL; part = part->next) {
        gd_tag *base = find_tag(inference, part->base);
        part->affixable = base != NULL && base->affixable;
    }
    for (part = inference->parts; part != NULL; part = part->next) {
        if (part->affixable) {
            gd_tag *word = gd_inference_ensure_tag(inference, part->name, err);
            if (word == NULL) return;
            word->kind = GD_ITEM; word->rarity = GD_COMMON;
            word->affixable = 1; word->item_present = 1;
            word->name_part = 1;
        }
    }
}

void gd_inference_free(gd_inference *inference)
{
    gd_tag *tag = inference->tags;
    gd_part *part = inference->parts;
    while (tag != NULL) { gd_tag *next = tag->next; free(tag->name); free(tag); tag = next; }
    while (part != NULL) { gd_part *next = part->next; free(part->name); free(part->base); free(part); part = next; }
    { gd_item_path *item = inference->item_paths;
      while (item != NULL) { gd_item_path *n = item->next; free(item->path); free(item->tag); free(item); item = n; } }
    { gd_loot_table *table = inference->loot_tables;
      while (table != NULL) {
          gd_loot_table *n = table->next;
          gd_loot_entry *entry = table->entries;
          while (entry != NULL) { gd_loot_entry *e = entry->next; free(entry->item_path); free(entry); entry = e; }
          free(table->path); free(table); table = n;
      } }
    free(inference->tag_buckets);
    free(inference->part_buckets);
    free(inference->item_path_buckets);
    free(inference->loot_table_buckets);
    gd_inference_init(inference);
}

const gd_tag *gd_tag_lookup(const gd_inference *inference, const char *name)
{
    return find_tag(inference, name);
}

/* gdse's own scheme colors only the rarities that can carry a name-altering
   affix, and only base names that can actually roll one. Full Rainbow instead
   colors every rarity on every name, paints style/quality words silver, and
   makes no exception for faction gear. */
static int decimal_suffix(const char *text)
{
    if (*text == '\0') return 0;
    while (*text != '\0') {
        if (*text < '0' || *text > '9') return 0;
        ++text;
    }
    return 1;
}

/* Full Rainbow does not paint crafting-result labels or Loyalist illusion
   equipment. These are localization categories rather than individual item
   exceptions: their database records carry ordinary rarity, but the labels
   describe a generated result or a cosmetic unlock instead of a normal drop. */
static int full_rainbow_omits_tag(const char *name)
{
    const char *suffix;
    if (starts(name, "tagCraftRandom")) return 1;
    if (starts(name, "tagDLCA")) suffix = name + strlen("tagDLCA");
    else if (starts(name, "tagDLCB")) suffix = name + strlen("tagDLCB");
    else return 0;
    return decimal_suffix(suffix);
}

/* Some localization categories encode their Full Rainbow role in the tag
   itself even when no item record owns the tag. */
static char full_rainbow_tag_color(const char *name)
{
    size_t len = strlen(name);
    if (strcmp(name, "tagStyleUniqueTier2") == 0) return 'a';
    if (strcmp(name, "tagStyleUniqueTier3") == 0) return 'p';
    if (strcmp(name, "tagItemTest") == 0) return 'f';
    if (starts(name, "tagQuestItem") &&
        !(len >= 4 && strcmp(name + len - 4, "Desc") == 0)) return 'g';
    return 0;
}

/* A few coherent localization families outlive the item records that once
   supplied their classification. Apply their family color only as a fallback;
   an inferred record always remains authoritative. Requiring an all-decimal
   suffix keeps description and other related tags untouched. */
static char full_rainbow_missing_tag_color(const char *name)
{
    static const char *silver[] = {"tagQualityWeaponWood"};
    static const char *green[] = {"tagShoulderF", "tagTorsoF"};
    static const char *white[] = {"tagHeadA", "tagTorsoM"};
    size_t i;
    for (i = 0; i < sizeof(silver) / sizeof(silver[0]); ++i)
        if (starts(name, silver[i]) &&
            decimal_suffix(name + strlen(silver[i]))) return 's';
    for (i = 0; i < sizeof(green) / sizeof(green[0]); ++i)
        if (starts(name, green[i]) &&
            decimal_suffix(name + strlen(green[i]))) return 'g';
    for (i = 0; i < sizeof(white) / sizeof(white[0]); ++i)
        if (starts(name, white[i]) &&
            decimal_suffix(name + strlen(white[i]))) return 'w';
    return 0;
}

static char tag_color_of(const gd_tag *tag, const char *name,
                         int full_rainbow)
{
    if (full_rainbow) {
        char category_color = full_rainbow_tag_color(name);
        if (category_color) return category_color;
        if (full_rainbow_omits_tag(name)) return 0;
    }
    if (tag == NULL)
        return full_rainbow ? full_rainbow_missing_tag_color(name) : 0;
    if (full_rainbow) {
        char missing_color = full_rainbow_missing_tag_color(name);
        if (tag->rarity == GD_UNKNOWN && missing_color) return missing_color;
        if (tag->rarity == GD_UNKNOWN && tag->broken_item) return 'w';
        if (tag->name_part) return 's';
        /* Monster Infrequents take their own colors at every tier. */
        if (tag->monster_infrequent) {
            if (tag->rarity == GD_EPIC) return 'z';
            if (tag->rarity == GD_LEGENDARY) return 'f';
            return 'l';
        }
        if (tag->rarity == GD_COMMON) return 'w';
        if (tag->rarity == GD_MAGICAL) return 'y';
        if (tag->rarity == GD_RARE) return 'g';
        if (tag->rarity == GD_EPIC) return 'b';
        if (tag->rarity == GD_LEGENDARY) return 'i';
        return 0;
    }
    if (tag->kind == GD_ITEM && !tag->affixable) return 0;
    if (tag->rarity == GD_COMMON) return 'w';
    if (tag->rarity == GD_MAGICAL) return 'y';
    if (tag->rarity == GD_RARE) return 'g';
    return 0;
}

char gd_tag_color(const gd_inference *inference, const char *name,
                  int full_rainbow)
{
    return tag_color_of(find_tag(inference, name), name, full_rainbow);
}

char gd_property_color(const char *tag, int rainbow)
{
    static const char *prefixes[] = {"Damage", "Defense", "Retaliation",
                                     "tagConversion", "tagDamageBase"};
    static const char *tokens[] = {"Physical", "Pierce", "Bleeding", "Fire",
        "Cold", "Lightning", "Poison", "Vitality", "Life", "Aether",
        "Chaos", "Elemental"};
    static const char colors[] = {'k','f','r','o','c','z','l','m','m','a','p','y'};
    size_t i;
    int valid = 0;
    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
        if (starts(tag, prefixes[i])) valid = 1;
    if (!valid || strstr(tag, "Reduction") != NULL ||
        starts(tag, "DamageModifierPierceRatio") ||
        (starts(tag, "Defense") && strstr(tag, "Duration") != NULL) ||
        strstr(tag, "Leech") != NULL || strstr(tag, "Leach") != NULL ||
        (strstr(tag, "Percent") != NULL && strstr(tag, "Life") != NULL)) return 0;
    for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
        if (strstr(tag, tokens[i]) != NULL)
            return rainbow && colors[i] == 'f' ? 'r' : colors[i];
    return 0;
}

static int color_at(const char *s)
{
    return s[0] == '{' && s[1] == '^' &&
           ((s[2] >= 'A' && s[2] <= 'Z') || (s[2] >= 'a' && s[2] <= 'z')) &&
           s[3] == '}';
}

static int ascii_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int has_letter(const char *s)
{
    gd_utf8proc_int32 codepoint;
    while (*s != '\0') {
        gd_utf8proc_ssize used = utf8proc_iterate((const unsigned char *)s, -1, &codepoint);
        int category;
        if (used < 0) { ++s; continue; }
        category = utf8proc_category(codepoint);
        if (category >= GD_UTF8PROC_LU && category <= GD_UTF8PROC_LO) return 1;
        s += used;
    }
    return 0;
}

static char *replace_all(const char *value, const char *needle,
                         const char *replacement, int color_codes,
                         gd_error *err)
{
    size_t cap = strlen(value) + 16;
    size_t len = 0;
    const char *p = value;
    size_t nlen = strlen(needle);
    char *out = (char *)gd_alloc(cap, err);
    if (out == NULL) return NULL;
    while (*p != '\0') {
        int match = color_codes ? color_at(p) : strncmp(p, needle, nlen) == 0;
        size_t take = match ? strlen(replacement) : 1;
        if (len + take + 1 > cap) {
            char *grown;
            cap = (cap + take + 16) * 2;
            grown = (char *)realloc(out, cap);
            if (grown == NULL) { free(out); gd_set_error(err, "out of memory"); return NULL; }
            out = grown;
        }
        if (match) { memcpy(out + len, replacement, take); len += take; p += color_codes ? 4 : nlen; }
        else out[len++] = *p++;
    }
    out[len] = '\0';
    return out;
}

char *gd_apply_color(const char *value, char color, gd_error *err)
{
    char cc[5] = {'{','^',0,'}','\0'};
    const char *p;
    char *out;
    size_t len = strlen(value), pos = 0, markers = 0, cap;
    cc[2] = (char)toupper((unsigned char)color);
    p = strstr(value, "{^E}");
    if (p == NULL) p = strstr(value, "{^S}");
    if (p != NULL) {
        char suffix[5]; memcpy(suffix, p, 4); suffix[4] = '\0';
        out = (char *)gd_alloc(len + 5, err); if (out == NULL) return NULL;
        memcpy(out, value, (size_t)(p - value)); memcpy(out + (p-value), cc, 4);
        strcpy(out + (p-value) + 4, p + 4); strcat(out, suffix); return out;
    }
    for (p = value; *p != '\0'; ++p) if (color_at(p)) return replace_all(value, "", cc, 1, err);
    if (value[0] == '[' || (value[0] == '$' && value[1] == '[')) {
        int in_bracket = 0, alphabetic = 0;
        for (p = value; *p != '\0'; ++p) {
            if (*p == '[') { in_bracket = 1; alphabetic = 0; }
            else if (in_bracket && *p == ']') {
                if (alphabetic) ++markers;
                in_bracket = 0;
            } else if (in_bracket && !ascii_alpha(*p)) in_bracket = 0;
            else if (in_bracket) alphabetic = 1;
        }
    } else if (value[0] == '|') {
        for (p = value; *p != '\0'; ++p)
            if (*p == '|' && isdigit((unsigned char)p[1])) { ++markers; ++p; }
    }
    if (len > (size_t)-1 - 5 ||
        markers > (((size_t)-1) - len - 5) / 4) {
        gd_set_error(err, "colored value is too large"); return NULL;
    }
    cap = len + markers * 4 + 5;
    out = (char *)gd_alloc(cap, err); if (out == NULL) return NULL;
    if (value[0] == '[' || (value[0] == '$' && value[1] == '[')) {
        int in_bracket = 0, alphabetic = 0;
        for (p = value; *p != '\0'; ++p) {
            out[pos++] = *p;
            if (*p == '[') { in_bracket = 1; alphabetic = 0; }
            else if (in_bracket && *p == ']') {
                if (alphabetic) { memcpy(out + pos, cc, 4); pos += 4; }
                in_bracket = 0;
            } else if (in_bracket && !ascii_alpha(*p)) in_bracket = 0;
            else if (in_bracket) alphabetic = 1;
        }
    } else if (value[0] == '$') {
        out[pos++] = '$'; memcpy(out + pos, cc, 4); pos += 4; strcpy(out + pos, value + 1); return out;
    } else if (value[0] == '|') {
        for (p = value; *p != '\0'; ++p) {
            out[pos++] = *p;
            if (*p == '|' && isdigit((unsigned char)p[1])) {
                out[pos++] = *++p; memcpy(out + pos, cc, 4); pos += 4;
            }
        }
    } else if (has_letter(value)) { memcpy(out, cc, 4); strcpy(out + 4, value); return out; }
    else { strcpy(out, value); return out; }
    out[pos] = '\0'; return out;
}

/* Full Rainbow prefixes set-item names with "(S) ", ahead of the color code. */
static char *prefix_set_marker(char *value, gd_error *err)
{
    static const char marker[] = "(S) ";
    size_t len = strlen(value);
    char *grown = (char *)realloc(value, len + sizeof(marker));
    if (grown == NULL) {
        free(value);
        gd_set_error(err, "out of memory");
        return NULL;
    }
    memmove(grown + sizeof(marker) - 1, grown, len + 1);
    memcpy(grown, marker, sizeof(marker) - 1);
    return grown;
}

char *gd_recolor_text(const char *text, size_t length,
                      const gd_inference *inference, int rainbow,
                      int full_rainbow, unsigned long *colored,
                      size_t *out_length, gd_error *err)
{
    size_t cap = length + 1, used = 0, start = 0;
    char *out = (char *)gd_alloc(cap, err);
    if (out == NULL) return NULL;
    *colored = 0;
    while (start < length) {
        size_t end = start, body_end, eq;
        int has_line_ending;
        char *tag, *value, *changed = NULL;
        const gd_tag *info;
        char color = 0;
        body_end = end;
        while (body_end < length && text[body_end] != '\r' &&
               text[body_end] != '\n') ++body_end;
        end = body_end;
        has_line_ending = end < length;
        if (has_line_ending) {
            if (text[end] == '\r' && end + 1 < length &&
                text[end + 1] == '\n') end += 2;
            else ++end;
        }
        eq = start; while (eq < body_end && text[eq] != '=') ++eq;
        if (eq < body_end) {
            tag = (char *)gd_alloc(eq-start+1, err);
            value = (char *)gd_alloc(body_end-eq, err);
            if (tag == NULL || value == NULL) { free(tag); free(value); free(out); return NULL; }
            memcpy(tag, text+start, eq-start); tag[eq-start] = '\0';
            memcpy(value, text+eq+1, body_end-eq-1); value[body_end-eq-1] = '\0';
            info = gd_tag_lookup(inference, tag);
            color = tag_color_of(info, tag, full_rainbow);
            if (!color) color = gd_property_color(tag, rainbow);
            if (color) changed = gd_apply_color(value, color, err);
            if (color && changed == NULL) {
                free(tag); free(value); free(out); return NULL;
            }
            if (changed != NULL && full_rainbow && info != NULL &&
                info->set_item) {
                changed = prefix_set_marker(changed, err);
                if (changed == NULL) { free(tag); free(value); free(out); return NULL; }
            }
            if (changed != NULL && strstr(tag, "Conversion") != NULL) {
                size_t n = strlen(changed); char *grown = (char *)realloc(changed, n+5);
                if (grown == NULL) { free(tag); free(value); free(changed); free(out); gd_set_error(err,"out of memory"); return NULL; }
                changed = grown; strcpy(changed+n, "{^E}");
            }
            if (changed != NULL && strcmp(changed, value) != 0) {
                size_t need = strlen(tag)+1+strlen(changed)+
                              (has_line_ending ? 2 : 0);
                if (used+need+1 > cap) { char *grown; cap=(used+need+1)*2; grown=(char *)realloc(out,cap); if(!grown){free(tag);free(value);free(changed);free(out);gd_set_error(err,"out of memory");return NULL;} out=grown; }
                memcpy(out+used,tag,strlen(tag)); used+=strlen(tag); out[used++]='=';
                memcpy(out+used,changed,strlen(changed)); used+=strlen(changed);
                if (has_line_ending) { out[used++]='\r'; out[used++]='\n'; }
                ++*colored;
                free(tag); free(value); free(changed); start=end; continue;
            }
            free(tag); free(value); free(changed);
        }
        if (used+(body_end-start)+(has_line_ending ? 2 : 0)+1 > cap) { char *grown; cap=(used+body_end-start+(has_line_ending ? 2 : 0)+1)*2; grown=(char *)realloc(out,cap); if(!grown){free(out);gd_set_error(err,"out of memory");return NULL;} out=grown; }
        memcpy(out+used,text+start,body_end-start); used+=body_end-start;
        if (has_line_ending) { out[used++]='\r'; out[used++]='\n'; }
        start=end;
    }
    out[used]='\0'; *out_length=used; return out;
}
