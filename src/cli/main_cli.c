/**
 * @file main_cli.c
 * @brief Program CLI rdr-cli untuk pemakaian headless dan pengujian E2E.
 *
 * Penggunaan:
 *   rdr-cli [--echo] [--strict] [script]
 *
 * Perintah dibaca per baris dari file script atau stdin. Baris kosong dan
 * baris yang diawali '#' diabaikan. Opsi --echo mencetak setiap perintah
 * sebelum hasilnya, dan --strict menghentikan program dengan exit code 1 pada
 * perintah pertama yang gagal. Ketik "help" untuk daftar perintah.
 *
 * CLI memakai controller App yang sama dengan GUI, sehingga setiap perilaku
 * yang diuji lewat CLI identik dengan perilaku GUI.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdr/app.h"
#include "rdr/platform.h"
#include "rdr/query.h"
#include "rdr/rdr_print.h"

/* ===================================================================== */
/* Output Helpers                                                        */
/* ===================================================================== */

static void print_status(const App *app)
{
    static const char *const LEVELS[] = {"info", "warning", "error"};
    size_t i;

    printf("[%s] %s\n", LEVELS[app->status_level], app->status);
    for (i = 0; i < app->details.count; i++) {
        printf("  - %s\n", app->details.items[i]);
    }
}

static void print_draft(const App *app)
{
    size_t i;
    char err[256];

    if (app->draft.count == 0) {
        printf("(empty query)\n");
        return;
    }
    for (i = 0; i < app->draft.count; i++) {
        int valid = app_draft_item_valid(app, i, err, sizeof(err));
        printf("%lu. %s%s%s\n", (unsigned long)(i + 1), app->draft.items[i],
               valid ? "" : "   <- invalid: ", valid ? "" : err);
    }
}

static void print_result(const App *app)
{
    const RdrInference *inf = &app->inference;
    size_t i;
    int show_rule = prefs_get(&app->prefs, PREF_RESULT_SHOW_RULE);

    if (!inf->valid) {
        printf("(no result)\n");
        return;
    }
    printf("Final conclusion: %s\n", app_conclusion(app));
    for (i = inf->trace.count; i-- > 0;) {
        const RdrNode *n = rdr_kb_find(app->kb, inf->trace.items[i]);
        char *rule = clauselist_rule_text(&n->rule);
        printf("  %s %s", i + 1 == inf->trace.count ? "*" : " ", n->conclusion);
        if (show_rule) {
            printf("   (#%d: %s)", n->id, rule);
        }
        printf("\n");
        free(rule);
    }
}

static void print_tree(const App *app)
{
    RdrPrintOptions opt;
    RdrLines lines;
    size_t i;
    int highlight = prefs_get(&app->prefs, PREF_TREE_HIGHLIGHT) && app->inference.valid;

    opt.show_empty_slots = prefs_get(&app->prefs, PREF_TREE_SHOW_EMPTY_SLOTS);
    opt.show_node_id = prefs_get(&app->prefs, PREF_TREE_SHOW_NODE_ID);
    opt.show_cornerstone = prefs_get(&app->prefs, PREF_TREE_SHOW_CORNERSTONE);
    rdr_lines_init(&lines);
    rdr_print_tree(app->kb, &opt, &lines);
    for (i = 0; i < lines.count; i++) {
        const RdrLine *l = &lines.items[i];
        const char *mark = "";
        if (highlight && l->kind == RDR_LINE_NODE) {
            RdrNodeStatus st = rdr_inference_status(&app->inference, l->node_id);
            if (l->node_id == app->inference.last_concluded) {
                mark = "   <== conclusion";
            } else if (st == RDR_NODE_FIRED) {
                mark = "   [fired]";
            } else if (st == RDR_NODE_FAILED) {
                mark = "   [failed]";
            }
        }
        printf("%s%s%s\n", l->prefix, l->text, mark);
    }
    rdr_lines_free(&lines);
}

static void print_plan(const App *app)
{
    AppCorrectionPlan plan;
    char *diff;

    if (!app_correction_plan(app, &plan)) {
        printf("(no current result to correct, run 'start' first)\n");
        app_correction_plan_free(&plan);
        return;
    }
    diff = query_format(&plan.difference);
    printf("Current conclusion: %s (node #%d)\n", plan.conclusion, plan.concluded_id);
    printf("Last evaluated node: #%d, new node goes to its %s slot\n", plan.evaluated_id,
           plan.attach_true ? "true" : "false");
    printf("Difference list: %s\n", plan.is_revision ? "(empty, revision case)" : diff);
    free(diff);
    app_correction_plan_free(&plan);
}

static void print_prefs(const App *app)
{
    int i;

    for (i = 0; i < PREF_COUNT; i++) {
        char *v = prefs_value_text((PrefId)i, prefs_get(&app->prefs, (PrefId)i));
        printf("%-32s %s\n", prefs_descriptor((PrefId)i)->key, v);
        free(v);
    }
}

static void print_help(void)
{
    printf(
        "Commands:\n"
        "  new <default>                  new knowledge base 'T >> <default>'\n"
        "  open <path> | save [path]      open or save a project file\n"
        "  add <clauses>                  append clauses to the query\n"
        "  query <clauses>                replace the query\n"
        "  draft | remove <n> | clear     show, remove clause n, or clear the query\n"
        "  start                          run the reasoning\n"
        "  result | tree | plan           show result, tree, or correction plan\n"
        "  correct <conclusion> [| <rule>]  correct the last result\n"
        "  rule <id> <clauses|T>          edit the rule of a node\n"
        "  conclusion <id> <text>         edit the conclusion of a node\n"
        "  cornerstone <id> <clauses>     edit the cornerstone of a node\n"
        "  add-child <id> true|false <rule|T> | <conclusion> [| <case>]\n"
        "  delete <id>                    delete a node and its subtree\n"
        "  validate                       check every cornerstone\n"
        "  undo | redo                    undo or redo the last change\n"
        "  import-csv <path> | import-prefs <path>\n"
        "  pref [<key> <value>]           list or set preferences\n"
        "  status | echo <text> | help | quit\n");
}

/* ===================================================================== */
/* Argument Helpers                                                      */
/* ===================================================================== */

/* Memisahkan teks pada '|' pertama. Bagian kanan NULL jika tidak ada. */
static char *split_bar(char *text)
{
    char *bar = strchr(text, '|');

    if (bar == NULL) {
        return NULL;
    }
    *bar = '\0';
    return bar + 1;
}

/* Mengambil kata pertama dari *rest dan memajukan *rest. */
static char *next_word(char **rest)
{
    char *p = *rest;
    char *word;

    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    word = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '\0') {
        *p++ = '\0';
    }
    *rest = p;
    return word;
}

static int parse_id(const char *text, int *out)
{
    char *end;
    long v = strtol(text, &end, 10);

    if (*text == '\0' || *end != '\0' || v < 0) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static void trim_inplace(char *s)
{
    char *t = str_trim_dup(s);
    strcpy(s, t);
    free(t);
}

/* ===================================================================== */
/* Command Dispatch                                                      */
/* ===================================================================== */

/* Menjalankan satu perintah. Mengembalikan 0 jika sukses, 1 jika gagal, 2 untuk quit. */
static int run_command(App *app, char *line)
{
    char *rest = line;
    char *cmd = next_word(&rest);
    char err[512];
    int id;

    trim_inplace(rest);

    /* Project Commands */
    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        return 2;
    }
    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(cmd, "echo") == 0) {
        printf("%s\n", rest);
        return 0;
    }
    if (strcmp(cmd, "status") == 0) {
        char *title = app_title(app);
        printf("%s | %lu nodes\n", title, (unsigned long)app->kb->node_count);
        free(title);
        print_status(app);
        return 0;
    }
    if (strcmp(cmd, "new") == 0) {
        int ok = app_new(app, rest);
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "open") == 0) {
        int ok = app_open(app, rest);
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "save") == 0) {
        int ok = rest[0] ? app_save_as(app, rest) : app_save(app);
        print_status(app);
        return ok ? 0 : 1;
    }

    /* Query Commands */
    if (strcmp(cmd, "add") == 0 || strcmp(cmd, "query") == 0) {
        if (strcmp(cmd, "query") == 0) {
            app_draft_clear(app);
        }
        app_draft_add(app, rest);
        print_draft(app);
        return 0;
    }
    if (strcmp(cmd, "draft") == 0) {
        print_draft(app);
        return 0;
    }
    if (strcmp(cmd, "remove") == 0) {
        if (!parse_id(rest, &id) || id < 1 || (size_t)id > app->draft.count) {
            printf("[error] remove expects a clause number between 1 and %lu\n",
                   (unsigned long)app->draft.count);
            return 1;
        }
        app_draft_remove(app, (size_t)(id - 1));
        print_draft(app);
        return 0;
    }
    if (strcmp(cmd, "clear") == 0) {
        app_clear(app);
        print_status(app);
        return 0;
    }
    if (strcmp(cmd, "start") == 0) {
        int ok = app_start(app);
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "undo") == 0 || strcmp(cmd, "redo") == 0) {
        int ok = cmd[0] == 'u' ? app_undo(app) : app_redo(app);
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "result") == 0) {
        print_result(app);
        return 0;
    }
    if (strcmp(cmd, "tree") == 0) {
        print_tree(app);
        return 0;
    }
    if (strcmp(cmd, "plan") == 0) {
        print_plan(app);
        return 0;
    }

    /* Acquisition Commands */
    if (strcmp(cmd, "correct") == 0) {
        char *rule_text = split_bar(rest);
        ClauseList rule;
        StrList errors;
        RdrAcqResult r;
        clauselist_init(&rule);
        strlist_init(&errors);
        if (rule_text != NULL && query_parse(rule_text, &rule, &errors) > 0) {
            printf("[error] %s\n", errors.items[0]);
            clauselist_free(&rule);
            strlist_free(&errors);
            return 1;
        }
        r = app_correct(app, rest, rule_text ? &rule : NULL);
        print_status(app);
        clauselist_free(&rule);
        strlist_free(&errors);
        return (r.status == RDR_ACQ_ERROR || r.status == RDR_ACQ_REJECTED) ? 1 : 0;
    }

    /* Manual Edit Commands */
    if (strcmp(cmd, "rule") == 0 || strcmp(cmd, "conclusion") == 0 ||
        strcmp(cmd, "cornerstone") == 0 || strcmp(cmd, "delete") == 0) {
        char *id_text = next_word(&rest);
        int ok;
        trim_inplace(rest);
        if (!parse_id(id_text, &id)) {
            printf("[error] '%s' expects a node id\n", cmd);
            return 1;
        }
        if (strcmp(cmd, "rule") == 0) {
            ok = app_edit_rule(app, id, rest, err, sizeof(err));
        } else if (strcmp(cmd, "conclusion") == 0) {
            ok = app_edit_conclusion(app, id, rest, err, sizeof(err));
        } else if (strcmp(cmd, "cornerstone") == 0) {
            ok = app_edit_cornerstone(app, id, rest, err, sizeof(err));
        } else {
            ok = app_delete_node(app, id, NULL, err, sizeof(err));
        }
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "add-child") == 0) {
        char *id_text = next_word(&rest);
        char *side_text = next_word(&rest);
        char *conclusion = split_bar(rest);
        char *case_text = conclusion ? split_bar(conclusion) : NULL;
        int ok;
        if (!parse_id(id_text, &id) || conclusion == NULL ||
            (strcmp(side_text, "true") != 0 && strcmp(side_text, "false") != 0)) {
            printf("[error] usage: add-child <id> true|false <rule|T> | <conclusion> [| <case>]\n");
            return 1;
        }
        ok = app_add_child(app, id, strcmp(side_text, "true") == 0 ? RDR_SIDE_TRUE : RDR_SIDE_FALSE,
                           rest, conclusion, case_text ? case_text : "", NULL, err, sizeof(err));
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "validate") == 0) {
        RdrValidation v;
        int errors;
        app_validate(app, &v);
        errors = (int)v.errors;
        rdr_validation_free(&v);
        print_status(app);
        return errors > 0 ? 1 : 0;
    }

    /* Import and Preference Commands */
    if (strcmp(cmd, "import-csv") == 0) {
        CsvImportReport report;
        size_t i;
        int ok;
        csv_report_init(&report);
        ok = app_import_csv(app, rest, &report);
        print_status(app);
        for (i = 0; i < report.messages.count; i++) {
            printf("  %s\n", report.messages.items[i]);
        }
        csv_report_free(&report);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "import-prefs") == 0) {
        int ok = app_import_prefs(app, rest);
        print_status(app);
        return ok ? 0 : 1;
    }
    if (strcmp(cmd, "pref") == 0) {
        char *key = next_word(&rest);
        PrefId pid;
        Preferences p = app->prefs;
        trim_inplace(rest);
        if (key[0] == '\0') {
            print_prefs(app);
            return 0;
        }
        if (!prefs_find(key, &pid)) {
            printf("[error] unknown preference '%s'\n", key);
            return 1;
        }
        if (!prefs_set_from_text(&p, pid, rest, err, sizeof(err))) {
            printf("[error] %s\n", err);
            return 1;
        }
        app_set_prefs(app, &p);
        print_status(app);
        return 0;
    }

    printf("[error] unknown command '%s', type 'help'\n", cmd);
    return 1;
}

/* ===================================================================== */
/* Main Program                                                          */
/* ===================================================================== */

int main(int argc, char **argv)
{
    App *app;
    FILE *in = stdin;
    int echo = 0;
    int strict = 0;
    int failures = 0;
    int i;
    StrBuf line;
    int ch;

    /* Argument Parsing */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--echo") == 0) {
            echo = 1;
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("usage: rdr-cli [--echo] [--strict] [script]\n");
            print_help();
            return 0;
        } else {
            in = platform_fopen(argv[i], "rb");
            if (in == NULL) {
                fprintf(stderr, "rdr-cli: cannot open '%s'\n", argv[i]);
                return 1;
            }
        }
    }

    /* Command Loop */
    app = app_create("Default");
    strbuf_init(&line);
    for (;;) {
        char *text;
        int rc;
        strbuf_clear(&line);
        while ((ch = fgetc(in)) != EOF && ch != '\n') {
            if (ch != '\r') {
                strbuf_append_char(&line, (char)ch);
            }
        }
        if (ch == EOF && line.len == 0) {
            break;
        }
        text = str_trim_dup(strbuf_cstr(&line));
        if (text[0] == '\0' || text[0] == '#') {
            free(text);
            continue;
        }
        if (echo) {
            printf("> %s\n", text);
        }
        rc = run_command(app, text);
        free(text);
        fflush(stdout);
        if (rc == 2) {
            break;
        }
        if (rc == 1) {
            failures++;
            if (strict) {
                break;
            }
        }
    }

    /* Cleanup */
    strbuf_free(&line);
    app_destroy(app);
    if (in != stdin) {
        fclose(in);
    }
    return (strict && failures > 0) ? 1 : 0;
}
