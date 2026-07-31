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
    t->kind=GD_ITEM; t->item_present=1; t->gear=1; t->set_item=1;
    ++t->counts[GD_EPIC];
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

int main(void)
{
    apply_cases(); property_cases(); rewrite_cases(); inference_cases();
    index_cases(); full_rainbow_cases();
    if(failures){fprintf(stderr,"%d tests failed\n",failures);return 1;}
    puts("all tests passed"); return 0;
}
