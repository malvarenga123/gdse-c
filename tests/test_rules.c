#include "gdse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect_string(const char *name, const char *actual, const char *expected)
{
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", name, expected,
                actual == NULL ? "NULL" : actual);
        ++failures;
    }
}

static void apply_cases(void)
{
    gd_error err;
    char *value;
    const char *inputs[] = {"Sword", "$Sword", "{^E}Sword", "{^S}Sword",
        "{^R}Sword{^E}", "[Skill]Sword", "$[Skill]Sword", "|1Sword", "123"};
    const char *outputs[] = {"{^G}Sword", "${^G}Sword", "{^G}Sword{^E}",
        "{^G}Sword{^S}", "{^R}Sword{^G}{^E}", "[Skill]{^G}Sword",
        "$[Skill]{^G}Sword", "|1{^G}Sword", "123"};
    size_t i;
    for (i=0;i<sizeof(inputs)/sizeof(inputs[0]);++i) {
        err.message[0]='\0'; value=gd_apply_color(inputs[i],'g',&err);
        expect_string(inputs[i],value,outputs[i]); free(value);
    }
    value=gd_apply_color("\xe5\x89\x91",'w',&err);
    expect_string("unicode alphabetic",value,"{^W}\xe5\x89\x91"); free(value);
    value=gd_apply_color("[ms]a[fs]b[ns]c[mp]d[fp]e[ms]f[fs]g[ns]h[mp]i",'g',&err);
    expect_string("many brackets",value,"[ms]{^G}a[fs]{^G}b[ns]{^G}c[mp]{^G}d[fp]{^G}e[ms]{^G}f[fs]{^G}g[ns]{^G}h[mp]{^G}i"); free(value);
    value=gd_apply_color("|1a|2b|3c|4d|5e|6f|7g|8h|9i",'g',&err);
    expect_string("many pipes",value,"|1{^G}a|2{^G}b|3{^G}c|4{^G}d|5{^G}e|6{^G}f|7{^G}g|8{^G}h|9{^G}i"); free(value);
    value=gd_apply_color("[12]Sword",'g',&err);
    expect_string("numeric brackets",value,"[12]Sword"); free(value);
    value=gd_apply_color("[]Sword",'g',&err);
    expect_string("empty brackets",value,"[]Sword"); free(value);
    value=gd_apply_color("[ms]Big] Sword",'g',&err);
    expect_string("stray close bracket",value,"[ms]{^G}Big] Sword"); free(value);
}

static void property_cases(void)
{
    if(gd_property_color("DamageFire",0)!='o')++failures;
    if(gd_property_color("DamagePierce",0)!='f')++failures;
    if(gd_property_color("DamagePierce",1)!='r')++failures;
    if(gd_property_color("DamageFireReduction",0)!=0)++failures;
    if(gd_property_color("DefenseFireDuration",0)!=0)++failures;
    if(gd_property_color("DamageLifeLeech",0)!=0)++failures;
}

static void rewrite_cases(void)
{
    gd_inference inference;
    gd_error err;
    unsigned long count;
    size_t length;
    char *out;
    gd_inference_init(&inference);
    out=gd_recolor_text("DamageFire=Burn\r\ncomment\r\n",
                        strlen("DamageFire=Burn\r\ncomment\r\n"),
                        &inference,0,0,&count,&length,&err);
    expect_string("CRLF",out,"DamageFire={^O}Burn\r\ncomment\r\n");
    if(count!=1)++failures;
    free(out);
    out=gd_recolor_text("DamageFire=Burn\ncomment\rDamageCold=Cold\r\n",
                        strlen("DamageFire=Burn\ncomment\rDamageCold=Cold\r\n"),
                        &inference,0,0,&count,&length,&err);
    expect_string("normalize line endings",out,
                  "DamageFire={^O}Burn\r\ncomment\r\n"
                  "DamageCold={^C}Cold\r\n");
    if(count!=2)++failures;
    free(out);
    out=gd_recolor_text("DamageCold=Cold",strlen("DamageCold=Cold"),
                        &inference,0,0,&count,&length,&err);
    expect_string("no final newline",out,"DamageCold={^C}Cold"); free(out);
    gd_inference_free(&inference);
}

static void inference_cases(void)
{
    gd_inference inference;
    gd_error err;
    gd_tag *base;
    gd_inference_init(&inference);
    base=gd_inference_ensure_tag(&inference,"base",&err);
    base->kind=GD_ITEM; base->gear=1;
    base->item_present=1;
    base->counts[GD_COMMON]=2; base->counts[GD_RARE]=2;
    if(!gd_inference_add_part(&inference,"style","base",&err))++failures;
    if(!gd_inference_add_part(&inference,"cascade","style",&err))++failures;
    gd_inference_finish(&inference,&err);
    if(base->rarity!=GD_COMMON || !base->affixable)++failures;
    if(gd_tag_color(&inference,"base",0)!='w')++failures;
    if(gd_tag_color(&inference,"style",0)!='w')++failures;
    if(gd_tag_color(&inference,"cascade",0)!=0)++failures;
    gd_inference_free(&inference);
}

static void index_cases(void)
{
    gd_inference inference;
    gd_error err;
    gd_tag *tag;
    char name[32];
    int i;
    gd_inference_init(&inference);
    for(i=0;i<20000;++i) {
        sprintf(name,"synthetic-tag-%d",i);
        tag=gd_inference_ensure_tag(&inference,name,&err);
        if(tag==NULL){++failures;break;}
        tag->kind=GD_AFFIX; tag->rarity=GD_RARE;
    }
    if(inference.tag_count!=20000 || inference.tag_bucket_count<20000)
        ++failures;
    for(i=0;i<20000;++i) {
        sprintf(name,"synthetic-tag-%d",i);
        if(gd_tag_color(&inference,name,0)!='g')++failures;
    }
    for(i=0;i<20000;++i)
        if(!gd_inference_add_part(&inference,"shared-part","shared-base",&err))
            ++failures;
    if(inference.part_count!=1)++failures;
    if(!gd_inference_add_part(&inference,"shared-part","other-base",&err))
        ++failures;
    if(inference.part_count!=2)++failures;
    gd_inference_free(&inference);
}

/* --full-rainbow reproduces four Full Rainbow categories that gdse's own
   scheme deliberately drops: Epic/Legendary names, the "(S) " set marker,
   faction gear, and silver style/quality words. */
static void full_rainbow_cases(void)
{
    gd_inference inference;
    gd_error err;
    gd_tag *t;
    unsigned long count;
    size_t length;
    char *out;
    gd_inference_init(&inference);

    t=gd_inference_ensure_tag(&inference,"epic",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_EPIC];
    t=gd_inference_ensure_tag(&inference,"legendary",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_LEGENDARY];
    t=gd_inference_ensure_tag(&inference,"setpiece",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1;
    t->set_records=1; t->name_records=1; ++t->counts[GD_EPIC];
    /* A tag shared by a base item and its Empowered/Mythical upgrades, where
       only one of the three tiers joins a set, is not a set name. */
    t=gd_inference_ensure_tag(&inference,"sharedtag",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1;
    t->set_records=1; t->name_records=3; ++t->counts[GD_EPIC];
    t=gd_inference_ensure_tag(&inference,"factiongear",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; t->faction=1;
    ++t->counts[GD_RARE];
    t=gd_inference_ensure_tag(&inference,"base",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_COMMON];
    if(!gd_inference_add_part(&inference,"quality","base",&err))++failures;
    gd_inference_finish(&inference,&err);

    /* gdse's own scheme: Epic/Legendary, set pieces and faction gear are all
       left to the engine, and a quality word takes the base's white. */
    if(gd_tag_color(&inference,"epic",0)!=0)++failures;
    if(gd_tag_color(&inference,"legendary",0)!=0)++failures;
    if(gd_tag_color(&inference,"factiongear",0)!=0)++failures;
    if(gd_tag_color(&inference,"quality",0)!='w')++failures;
    /* Full Rainbow: every rarity colored, faction included, quality silver. */
    if(gd_tag_color(&inference,"epic",1)!='b')++failures;
    if(gd_tag_color(&inference,"legendary",1)!='i')++failures;
    if(gd_tag_color(&inference,"factiongear",1)!='g')++failures;
    if(gd_tag_color(&inference,"quality",1)!='s')++failures;
    if(gd_tag_color(&inference,"base",1)!='w')++failures;

    out=gd_recolor_text("setpiece=Explorer's Footpads\r\n",
                        strlen("setpiece=Explorer's Footpads\r\n"),
                        &inference,0,1,&count,&length,&err);
    expect_string("set marker",out,"setpiece=(S) {^B}Explorer's Footpads\r\n");
    if(count!=1)++failures;
    free(out);
    out=gd_recolor_text("sharedtag=Soiled Trousers\r\n",
                        strlen("sharedtag=Soiled Trousers\r\n"),
                        &inference,0,1,&count,&length,&err);
    expect_string("minority set records",out,
                  "sharedtag={^B}Soiled Trousers\r\n");
    free(out);
    /* The marker is Full Rainbow's alone; gdse's own scheme never emits it. */
    out=gd_recolor_text("setpiece=Explorer's Footpads\r\n",
                        strlen("setpiece=Explorer's Footpads\r\n"),
                        &inference,0,0,&count,&length,&err);
    expect_string("no set marker by default",out,
                  "setpiece=Explorer's Footpads\r\n");
    if(count!=0)++failures;
    free(out);
    gd_inference_free(&inference);
}

/* Monster Infrequent inference: an item in a loot table a monster names in its
   own drop slots takes Full Rainbow's MI color for its tier. */
static void monster_infrequent_cases(void)
{
    gd_inference inference;
    gd_error err;
    gd_tag *t;
    gd_loot_table *table;
    gd_inference_init(&inference);

    t=gd_inference_ensure_tag(&inference,"tagYetiHorn",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_RARE];
    if(!add_item_path_for_test(&inference,"records/items/f/yeti.dbr",
                               "tagYetiHorn",&err))++failures;
    t=gd_inference_ensure_tag(&inference,"tagBossBlade",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_LEGENDARY];
    if(!add_item_path_for_test(&inference,"records/items/w/boss.dbr",
                               "tagBossBlade",&err))++failures;
    t=gd_inference_ensure_tag(&inference,"tagSabre",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_COMMON];
    if(!add_item_path_for_test(&inference,"records/items/w/sabre.dbr",
                               "tagSabre",&err))++failures;
    /* Common gear sitting in a monster's own loot slot is what it wields, not
       an Infrequent. */
    t=gd_inference_ensure_tag(&inference,"tagWieldedClub",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_COMMON];
    if(!add_item_path_for_test(&inference,"records/items/w/club.dbr",
                               "tagWieldedClub",&err))++failures;

    /* Two monster-attached tables and one the monster never names. */
    if(!add_loot_entry_for_test(&inference,"records/items/loottables/t_yeti.dbr",
                                "records/items/f/yeti.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,"records/items/loottables/t_boss.dbr",
                                "records/items/w/boss.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,"records/items/loottables/t_craft.dbr",
                                "records/items/w/sabre.dbr",&err))++failures;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/t_yeti.dbr",&err);
    if(table==NULL)++failures; else table->monster_drop=1;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/t_boss.dbr",&err);
    if(table==NULL)++failures; else table->monster_drop=1;
    if(!add_loot_entry_for_test(&inference,"records/items/loottables/t_yeti.dbr",
                                "records/items/w/club.dbr",&err))++failures;

    /* Nested tables: the creature names lt_nested, which holds tdyn_nested,
       which holds tdyn_deep, which finally holds the item. Alongside it sits a
       master table, which must not be dragged in by the nesting. */
    t=gd_inference_ensure_tag(&inference,"tagNestedClaw",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_EPIC];
    if(!add_item_path_for_test(&inference,"records/items/w/nested.dbr",
                               "tagNestedClaw",&err))++failures;
    t=gd_inference_ensure_tag(&inference,"tagWorldRare",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_RARE];
    if(!add_item_path_for_test(&inference,"records/items/w/world.dbr",
                               "tagWorldRare",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/tdyn_deep.dbr",
                                "records/items/w/nested.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/tdyn_nested.dbr",
                                "records/items/loottables/tdyn_deep.dbr",
                                &err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/mastertables/mt_all.dbr",
                                "records/items/w/world.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/lt_nested.dbr",
                                "records/items/loottables/tdyn_nested.dbr",
                                &err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/lt_nested.dbr",
                                "records/items/loottables/mastertables/mt_all.dbr",
                                &err))++failures;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/lt_nested.dbr",&err);
    if(table==NULL)++failures; else {table->monster_drop=1;table->expandable=1;}
    /* Named by far more creatures than any one monster's own table: a shared
       world pool, and the nesting must stop at it. */
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/mastertables/mt_all.dbr",&err);
    if(table==NULL)++failures; else table->creature_refs=GD_SHARED_TABLE_REFS+1;
    /* A monster family names its own table from every variant record it has,
       so a high count outside the mastertables layer means nothing. */
    t=gd_inference_ensure_tag(&inference,"tagFamilyDrop",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_RARE];
    if(!add_item_path_for_test(&inference,"records/items/w/family.dbr",
                               "tagFamilyDrop",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/t_family.dbr",
                                "records/items/w/family.dbr",&err))++failures;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/t_family.dbr",&err);
    if(table==NULL)++failures; else table->creature_refs=40;
    /* Inside that layer the count is what separates one boss's own table from
       a world pool. */
    t=gd_inference_ensure_tag(&inference,"tagBossOwn",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_RARE];
    if(!add_item_path_for_test(&inference,"records/items/w/bossown.dbr",
                               "tagBossOwn",&err))++failures;
    t=gd_inference_ensure_tag(&inference,"tagPoolDrop",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_RARE];
    if(!add_item_path_for_test(&inference,"records/items/w/pool.dbr",
                               "tagPoolDrop",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/mastertables/mt_boss.dbr",
                                "records/items/w/bossown.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/mastertables/mt_pool.dbr",
                                "records/items/w/pool.dbr",&err))++failures;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/mastertables/mt_boss.dbr",&err);
    if(table==NULL)++failures; else table->creature_refs=1;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/mastertables/mt_pool.dbr",&err);
    if(table==NULL)++failures; else table->creature_refs=50;

    /* A creature also names generic tables for the gear it wields. Those are
       wrappers over the whole tier and must not be followed, or every Epic in
       the game reads as somebody's Infrequent. */
    t=gd_inference_ensure_tag(&inference,"tagWorldEpic",&err);
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; ++t->counts[GD_EPIC];
    if(!add_item_path_for_test(&inference,"records/items/w/worldepic.dbr",
                               "tagWorldEpic",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/tdyn_epic_pool.dbr",
                                "records/items/w/worldepic.dbr",&err))++failures;
    if(!add_loot_entry_for_test(&inference,
                                "records/items/loottables/lt_wielded_c01.dbr",
                                "records/items/loottables/tdyn_epic_pool.dbr",
                                &err))++failures;
    table=ensure_loot_table_for_test(&inference,
                                     "records/items/loottables/lt_wielded_c01.dbr",&err);
    if(table==NULL)++failures; else table->creature_refs=1;

    /* A LevelTable's `records` string array arrives as one field per element;
       a .dbr text export joins them with semicolons instead. Both must read. */
    {
        gd_record rec;
        gd_field f,f2;
        f2.key="records";
        f2.value="records/items/loottables/tdyn_lvl_a.dbr";
        f2.next=NULL;
        f.key="records";
        f.value="records/items/loottables/tdyn_lvl_b.dbr";
        f.next=&f2;
        rec.id="records/items/loottables/lt_level.dbr";
        rec.fields=&f;
        if(!scan_loot_table_for_test(&inference,&rec,&err))++failures;
        table=find_loot_table_for_test(&inference,
                                       "records/items/loottables/lt_level.dbr");
        if(table==NULL||table->entries==NULL||table->entries->next==NULL||
           table->entries->next->next!=NULL)++failures;
        else{
            int seen_a=0,seen_b=0;
            gd_loot_entry *e;
            for(e=table->entries;e!=NULL;e=e->next){
                if(strcmp(e->item_path,
                          "records/items/loottables/tdyn_lvl_a.dbr")==0)++seen_a;
                if(strcmp(e->item_path,
                          "records/items/loottables/tdyn_lvl_b.dbr")==0)++seen_b;
            }
            if(seen_a!=1||seen_b!=1)++failures;
        }
    }

    gd_inference_finish(&inference,&err);

    if(gd_tag_color(&inference,"tagYetiHorn",1)!='l')++failures;
    if(gd_tag_color(&inference,"tagBossBlade",1)!='f')++failures;
    /* Reached only through a table no monster names: an ordinary base. */
    if(gd_tag_color(&inference,"tagSabre",1)!='w')++failures;
    /* In a monster's own table, but Common: still an ordinary base. */
    if(gd_tag_color(&inference,"tagWieldedClub",1)!='w')++failures;
    /* Two table hops from the creature's own table: still an Infrequent. */
    if(gd_tag_color(&inference,"tagNestedClaw",1)!='z')++failures;
    /* Behind a shared pool the nesting must not follow: an ordinary Rare. */
    if(gd_tag_color(&inference,"tagWorldRare",1)!='g')++failures;
    /* Seeded without any monster_drop set by hand: a family table survives its
       own variant count, a boss's mastertable is admitted, a pool is not. */
    if(gd_tag_color(&inference,"tagFamilyDrop",1)!='l')++failures;
    if(gd_tag_color(&inference,"tagBossOwn",1)!='l')++failures;
    if(gd_tag_color(&inference,"tagPoolDrop",1)!='g')++failures;
    /* Two hops behind a creature-named wield table: an ordinary Epic. */
    if(gd_tag_color(&inference,"tagWorldEpic",1)!='b')++failures;
    /* gdse's own scheme never emits MI colors. */
    if(gd_tag_color(&inference,"tagYetiHorn",0)!='g')++failures;
    if(gd_tag_color(&inference,"tagBossBlade",0)!=0)++failures;
    gd_inference_free(&inference);
}

int main(void)
{
    apply_cases(); property_cases(); rewrite_cases(); inference_cases();
    index_cases(); full_rainbow_cases(); monster_infrequent_cases();
    if(failures){fprintf(stderr,"%d tests failed\n",failures);return 1;}
    puts("all tests passed"); return 0;
}
