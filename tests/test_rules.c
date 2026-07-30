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
                        &inference,0,&count,&length,&err);
    expect_string("CRLF",out,"DamageFire={^O}Burn\r\ncomment\r\n");
    if(count!=1)++failures;
    free(out);
    out=gd_recolor_text("DamageCold=Cold",strlen("DamageCold=Cold"),
                        &inference,0,&count,&length,&err);
    expect_string("no final newline",out,"DamageCold={^C}Cold"); free(out);
    gd_inference_free(&inference);
}

static void inference_cases(void)
{
    gd_inference inference;
    gd_error err;
    gd_tag *base;
    gd_part *part, *part2;
    gd_inference_init(&inference);
    base=(gd_tag *)calloc(1,sizeof(*base));
    part=(gd_part *)calloc(1,sizeof(*part));
    part2=(gd_part *)calloc(1,sizeof(*part2));
    base->name=gd_strdup("base",&err); base->kind=GD_ITEM; base->gear=1;
    base->item_present=1;
    base->counts[GD_COMMON]=2; base->counts[GD_RARE]=2;
    part->name=gd_strdup("style",&err); part->base=gd_strdup("base",&err);
    part2->name=gd_strdup("cascade",&err); part2->base=gd_strdup("style",&err);
    part->next=part2; inference.tags=base; inference.parts=part;
    gd_inference_finish(&inference,&err);
    if(base->rarity!=GD_COMMON || !base->affixable)++failures;
    if(gd_tag_color(&inference,"base")!='w')++failures;
    if(gd_tag_color(&inference,"style")!='w')++failures;
    if(gd_tag_color(&inference,"cascade")!=0)++failures;
    gd_inference_free(&inference);
}

int main(void)
{
    apply_cases(); property_cases(); rewrite_cases(); inference_cases();
    if(failures){fprintf(stderr,"%d tests failed\n",failures);return 1;}
    puts("all tests passed"); return 0;
}
