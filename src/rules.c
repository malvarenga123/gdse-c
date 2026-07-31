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
    inference->tag_buckets = NULL;
    inference->part_buckets = NULL;
    inference->tag_bucket_count = 0;
    inference->part_bucket_count = 0;
    inference->tag_count = 0;
    inference->part_count = 0;
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
        if (!starts(record.id, "records/items/")) { gd_record_free(&record); continue; }
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
            tag_name = gd_record_field(&record, "itemNameTag");
            if (tag_name == NULL || *tag_name == '\0') { gd_record_free(&record); continue; }
            tag = gd_inference_ensure_tag(inference, tag_name, err);
            if (tag == NULL) { gd_record_free(&record); return 0; }
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
        }
    }
}

void gd_inference_free(gd_inference *inference)
{
    gd_tag *tag = inference->tags;
    gd_part *part = inference->parts;
    while (tag != NULL) { gd_tag *next = tag->next; free(tag->name); free(tag); tag = next; }
    while (part != NULL) { gd_part *next = part->next; free(part->name); free(part->base); free(part); part = next; }
    free(inference->tag_buckets);
    free(inference->part_buckets);
    gd_inference_init(inference);
}

char gd_tag_color(const gd_inference *inference, const char *name)
{
    const gd_tag *tag = find_tag(inference, name);
    if (tag != NULL) {
        if (tag->kind == GD_ITEM && !tag->affixable) return 0;
        if (tag->rarity == GD_COMMON) return 'w';
        if (tag->rarity == GD_MAGICAL) return 'y';
        if (tag->rarity == GD_RARE) return 'g';
        return 0;
    }
    return 0;
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

char *gd_recolor_text(const char *text, size_t length,
                      const gd_inference *inference, int rainbow,
                      unsigned long *colored, size_t *out_length,
                      gd_error *err)
{
    size_t cap = length + 1, used = 0, start = 0;
    char *out = (char *)gd_alloc(cap, err);
    if (out == NULL) return NULL;
    *colored = 0;
    while (start < length) {
        size_t end = start, body_end, eq;
        int has_line_ending;
        char *tag, *value, *changed = NULL;
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
            color = gd_tag_color(inference, tag); if (!color) color = gd_property_color(tag, rainbow);
            if (color) changed = gd_apply_color(value, color, err);
            if (color && changed == NULL) {
                free(tag); free(value); free(out); return NULL;
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
