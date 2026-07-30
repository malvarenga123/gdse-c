#include "gdse.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define GD_RMDIR _rmdir
#else
#include <unistd.h>
#define GD_RMDIR rmdir
#endif

typedef struct output_file {
    char *name;
    int backed_up;
    int published;
    struct output_file *next;
} output_file;

static void usage(FILE *stream)
{
    fprintf(stream, "Usage: gdse GRIM_DAWN_INSTALL_PATH\n");
    fprintf(stream, "            [-l LANG|--language LANG] [-o PATH|--out PATH]\n");
    fprintf(stream, "            [--rainbow-filter-damage-colors]\n");
}

static int ends_with(const char *text, const char *suffix)
{
    size_t a = strlen(text), b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

static int has_output(output_file *files, const char *name)
{
    while (files != NULL) {
        if (strcmp(files->name, name) == 0) return 1;
        files = files->next;
    }
    return 0;
}

static int add_output(output_file **files, const char *name, gd_error *err)
{
    output_file *file;
    if (has_output(*files, name)) {
        gd_set_error(err, "multiple archive records target %s", name);
        return 0;
    }
    file = (output_file *)gd_alloc(sizeof(*file), err);
    if (file == NULL) return 0;
    file->name = gd_strdup(name, err);
    if (file->name == NULL) { free(file); return 0; }
    file->backed_up = 0; file->published = 0;
    file->next = *files; *files = file;
    return 1;
}

static void free_outputs(output_file *files)
{
    while (files != NULL) { output_file *next=files->next; free(files->name); free(files); files=next; }
}

static int parent_dirs(const char *path, gd_error *err)
{
    char *copy = gd_strdup(path, err);
    char *slash;
    int result;
    if (copy == NULL) return 0;
    slash = strrchr(copy, '/');
    if (slash == NULL) slash = strrchr(copy, '\\');
    if (slash == NULL) { free(copy); return 1; }
    *slash = '\0'; result = gd_mkdirs(copy, err); free(copy); return result;
}

static void prune_parents(const char *path, const char *stop)
{
    char *copy = (char *)malloc(strlen(path) + 1);
    if (copy == NULL) return;
    strcpy(copy, path);
    for (;;) {
        char *a = strrchr(copy, '/');
        char *b = strrchr(copy, '\\');
        char *slash = a == NULL ? b : (b == NULL || a > b ? a : b);
        if (slash == NULL) break;
        *slash = '\0';
        if (strcmp(copy, stop) == 0) break;
        GD_RMDIR(copy);
    }
    free(copy);
}

static int write_file(const char *path, const void *data, size_t length,
                      gd_error *err)
{
    FILE *file;
    if (!parent_dirs(path, err)) return 0;
    file = fopen(path, "wb");
    if (file == NULL) {
        gd_set_error(err, "could not write %s", path);
        return 0;
    }
    if (fwrite(data, 1, length, file) != length) {
        fclose(file);
        gd_set_error(err, "could not write %s", path);
        return 0;
    }
    if (fclose(file) != 0) {
        gd_set_error(err, "could not finish %s", path);
        return 0;
    }
    return 1;
}

static int load_database(const char *install, const char *rel, int required,
                         gd_inference *inference, gd_error *err)
{
    char *path = gd_path_join(install, rel, err);
    gd_arz *db;
    int ok;
    if (path == NULL) return 0;
    if (!gd_path_exists(path)) {
        if (required) gd_set_error(err, "required database is missing: %s", path);
        free(path); return required ? 0 : 1;
    }
    db = gd_arz_open(path, err); free(path);
    if (db == NULL) return 0;
    ok = gd_infer_database(inference, db, err);
    gd_arz_close(db);
    return ok;
}

static int process_archive(const char *install, const char *rel, int required,
                           const char *stage, const gd_inference *inference,
                           int rainbow, output_file **outputs, gd_error *err)
{
    char *path = gd_path_join(install, rel, err);
    gd_arc *arc;
    gd_u32 i;
    if (path == NULL) return 0;
    if (!gd_path_exists(path)) {
        if (required) gd_set_error(err, "required language archive is missing: %s", path);
        free(path); return required ? 0 : 1;
    }
    arc = gd_arc_open(path, err); free(path);
    if (arc == NULL) return 0;
    for (i = 0; i < gd_arc_count(arc); ++i) {
        char *id;
        gd_u8 *data;
        size_t length, new_length;
        unsigned long colored;
        char *rewritten;
        char *destination;
        if (!gd_arc_record(arc, i, &id, &data, &length, err)) { gd_arc_close(arc); return 0; }
        if (strstr(id, "tag") == NULL || !ends_with(id, ".txt")) { free(id); free(data); continue; }
        if (!gd_safe_record_path(id)) { gd_set_error(err, "unsafe archive path: %s", id); free(id); free(data); gd_arc_close(arc); return 0; }
        rewritten = gd_recolor_text((const char *)data, length, inference,
                                    rainbow, &colored, &new_length, err);
        free(data);
        if (rewritten == NULL) { free(id); gd_arc_close(arc); return 0; }
        if (colored == 0) { free(id); free(rewritten); continue; }
        if (!add_output(outputs, id, err)) { free(id); free(rewritten); gd_arc_close(arc); return 0; }
        destination = gd_path_join(stage, id, err);
        if (destination == NULL || !write_file(destination, rewritten, new_length, err)) {
            free(destination); free(id); free(rewritten); gd_arc_close(arc); return 0;
        }
        printf("%s (%lu tags)\n", id, colored);
        free(destination); free(id); free(rewritten);
    }
    gd_arc_close(arc); return 1;
}

static int publish(const char *out, const char *stage, output_file *outputs,
                   gd_error *err)
{
    output_file *file;
    char *manifest = gd_path_join(out, ".gdse-manifest", err);
    char *stage_manifest = gd_path_join(stage, ".gdse-manifest", err);
    FILE *mf;
    if (manifest == NULL || stage_manifest == NULL) goto fail;
    mf = fopen(stage_manifest, "wb");
    if (mf == NULL) { gd_set_error(err, "could not create output manifest"); goto fail; }
    for (file = outputs; file != NULL; file = file->next) fprintf(mf, "%s\n", file->name);
    if (fclose(mf) != 0) { gd_set_error(err, "could not finish output manifest"); goto fail; }
    if (!gd_mkdirs(out, err)) goto fail;
    mf = fopen(manifest, "rb");
    if (mf != NULL) {
        char line[4096];
        while (fgets(line, sizeof(line), mf) != NULL) {
            size_t len = strlen(line); char *old;
            while (len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
            if (*line && gd_safe_record_path(line) && !has_output(outputs, line)) {
                old = gd_path_join(out, line, err); if (old != NULL) { remove(old); free(old); }
            }
        }
        fclose(mf);
    }
    for (file = outputs; file != NULL; file = file->next) {
        char *from = gd_path_join(stage, file->name, err);
        char *to = gd_path_join(out, file->name, err);
        char *backup_rel = (char *)gd_alloc(strlen(file->name)+9,err);
        char *backup;
        if(backup_rel==NULL){free(from);free(to);goto rollback;}
        sprintf(backup_rel,".backup/%s",file->name);
        backup=gd_path_join(stage,backup_rel,err); free(backup_rel);
        if (from == NULL || to == NULL || backup == NULL || !parent_dirs(to, err) ||
            !parent_dirs(backup,err)) { free(from); free(to); free(backup); goto rollback; }
        if(gd_path_exists(to)){ if(rename(to,backup)!=0){gd_set_error(err,"could not back up %s",to);free(from);free(to);free(backup);goto rollback;} file->backed_up=1; }
        if (rename(from, to) != 0) { gd_set_error(err, "could not publish %s: %s", to, strerror(errno)); free(from); free(to); free(backup); goto rollback; }
        file->published=1; free(from); free(to); free(backup);
    }
    remove(manifest);
    if (rename(stage_manifest, manifest) != 0) { gd_set_error(err, "could not publish manifest"); goto fail; }
    for(file=outputs;file!=NULL;file=file->next){
        char *source=gd_path_join(stage,file->name,err);
        char *backup_rel=(char *)gd_alloc(strlen(file->name)+9,err);
        char *backup=NULL;
        if(backup_rel!=NULL){sprintf(backup_rel,".backup/%s",file->name);backup=gd_path_join(stage,backup_rel,err);free(backup_rel);}
        if(backup!=NULL)remove(backup);
        if(source!=NULL)prune_parents(source,stage);
        if(backup!=NULL)prune_parents(backup,stage);
        free(source);free(backup);
    }
    { char *backup_root=gd_path_join(stage,".backup",err); if(backup_root!=NULL){GD_RMDIR(backup_root);free(backup_root);} }
    GD_RMDIR(stage);
    free(manifest); free(stage_manifest); return 1;
rollback:
    for(file=outputs;file!=NULL;file=file->next){
        char *to=gd_path_join(out,file->name,err);
        char *backup_rel=(char *)gd_alloc(strlen(file->name)+9,err);
        char *backup=NULL;
        if(backup_rel!=NULL){sprintf(backup_rel,".backup/%s",file->name);backup=gd_path_join(stage,backup_rel,err);free(backup_rel);}
        if(to!=NULL && file->published)remove(to);
        if(to!=NULL && backup!=NULL && file->backed_up)rename(backup,to);
        free(to);free(backup);
    }
fail:
    free(manifest); free(stage_manifest); return 0;
}

int main(int argc, char **argv)
{
    static const char *dbs[] = {"database/database.arz", "gdx1/database/GDX1.arz",
        "gdx2/database/GDX2.arz", "gdx3/database/GDX3.arz"};
    const char *language = "en", *out_arg = NULL, *install = NULL;
    char lang[64], arc_rel[256], *out = NULL, *stage = NULL;
    int rainbow = 0, i, ok = 0;
    gd_error err; gd_inference inference; output_file *outputs = NULL;
    err.message[0] = '\0';
    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--language") == 0) && i+1 < argc) language=argv[++i];
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--out") == 0) && i+1 < argc) out_arg=argv[++i];
        else if (strcmp(argv[i], "--rainbow-filter-damage-colors") == 0) rainbow=1;
        else if (strcmp(argv[i], "--version") == 0) { puts("gdse 0.1.0-c89"); return 0; }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(stdout); return 0; }
        else if (argv[i][0] != '-' && install == NULL) install=argv[i];
        else { usage(stderr); return 2; }
    }
    if (install == NULL) { fprintf(stderr,"GRIM_DAWN_INSTALL_PATH argument is required\n"); usage(stderr); return 2; }
    if (strlen(language) >= sizeof(lang)) { fprintf(stderr,"language is too long\n"); return 2; }
    strcpy(lang, language); for (i=0; lang[i]; ++i) lang[i]=(char)tolower((unsigned char)lang[i]);
    if (!gd_is_directory(install)) { fprintf(stderr,"GRIM_DAWN_INSTALL_PATH must name an existing directory: %s\n",install); return 1; }
    if (out_arg != NULL) out=gd_strdup(out_arg,&err);
    else { char settings[128]; sprintf(settings,"settings/text_%s",lang); out=gd_path_join(install,settings,&err); }
    if (out == NULL) goto done;
    stage=(char *)gd_alloc(strlen(out)+20,&err); if(stage==NULL)goto done;
    sprintf(stage,"%s.gdse-stage",out);
    if (gd_path_exists(stage)) { gd_set_error(&err,"staging path already exists: %s",stage); goto done; }
    if (!gd_mkdirs(stage,&err)) goto done;
    gd_inference_init(&inference);
    for(i=0;i<4;++i) if(!load_database(install,dbs[i],i==0,&inference,&err))goto cleanup_inference;
    gd_inference_finish(&inference,&err); if(err.message[0])goto cleanup_inference;
    for(i=0;i<4;++i){ const char *prefix=i==0?"":i==1?"gdx1/":i==2?"gdx2/":"gdx3/"; sprintf(arc_rel,"%sresources/Text_",prefix); { size_t n=strlen(arc_rel),j; for(j=0;lang[j];++j)arc_rel[n+j]=(char)toupper((unsigned char)lang[j]); strcpy(arc_rel+n+j,".arc"); } if(!process_archive(install,arc_rel,i==0,stage,&inference,rainbow,&outputs,&err))goto cleanup_inference; }
    if(!publish(out,stage,outputs,&err))goto cleanup_inference;
    ok=1;
cleanup_inference:
    gd_inference_free(&inference);
done:
    if(!ok) fprintf(stderr,"gdse: %s\n",err.message[0]?err.message:"generation failed");
    if(!ok && stage!=NULL){ output_file *f; for(f=outputs;f!=NULL;f=f->next){ char *p=gd_path_join(stage,f->name,&err); if(p!=NULL){remove(p);prune_parents(p,stage);free(p);} } { char *m=gd_path_join(stage,".gdse-manifest",&err); if(m!=NULL){remove(m);free(m);} } GD_RMDIR(stage); }
    free_outputs(outputs); free(stage); free(out); return ok?0:1;
}
