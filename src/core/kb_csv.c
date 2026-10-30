/**
 * @file kb_csv.c
 * @brief Implementasi akuisisi knowledge dari dataset CSV.
 */

#include "rdr/kb_csv.h"

#include <stdlib.h>
#include <string.h>

#include "rdr/csv.h"
#include "rdr/platform.h"
#include "rdr/query.h"

void csv_report_init(CsvImportReport *r)
{
    memset(r, 0, sizeof(*r));
    strlist_init(&r->messages);
}

void csv_report_free(CsvImportReport *r)
{
    strlist_free(&r->messages);
    csv_report_init(r);
}

char *csv_report_summary(const CsvImportReport *r)
{
    return str_printf("%lu rows: %lu already correct, %lu exceptions added, %lu revisions added, "
                      "%lu conclusions edited, %lu rejected, %lu failed",
                      (unsigned long)r->rows, (unsigned long)r->already_correct,
                      (unsigned long)r->exceptions_added, (unsigned long)r->revisions_added,
                      (unsigned long)r->conclusions_edited, (unsigned long)r->rejected,
                      (unsigned long)r->failed);
}

static int header_matches(const StrList *row)
{
    char *a;
    char *b;
    int ok;

    if (row->count != 2) {
        return 0;
    }
    a = str_trim_dup(row->items[0]);
    b = str_trim_dup(row->items[1]);
    ok = str_iequals(a, "case") && str_iequals(b, "conclusion");
    free(a);
    free(b);
    return ok;
}

/* Memproses satu baris data dan memperbarui laporan. */
static void import_row(RdrKb *kb, const StrList *row, size_t line, RdrRevisionStrategy strategy,
                       CsvImportReport *r, RdrInference *inf)
{
    ClauseList query;
    StrList errors;
    char *conclusion;
    RdrAcqResult res;

    if (row->count != 2) {
        r->failed++;
        strlist_pushf(&r->messages, "line %lu: expected 2 columns, found %lu",
                      (unsigned long)line, (unsigned long)row->count);
        return;
    }

    /* Row Parsing */
    clauselist_init(&query);
    strlist_init(&errors);
    conclusion = str_trim_dup(row->items[1]);
    if (query_parse(row->items[0], &query, &errors) > 0) {
        r->failed++;
        strlist_pushf(&r->messages, "line %lu: %s", (unsigned long)line, errors.items[0]);
        goto done;
    }
    if (conclusion[0] == '\0') {
        r->failed++;
        strlist_pushf(&r->messages, "line %lu: the conclusion is empty", (unsigned long)line);
        goto done;
    }

    /* Inference */
    if (!rdr_infer(kb, &query, inf, &errors)) {
        r->failed++;
        strlist_pushf(&r->messages, "line %lu: %s", (unsigned long)line, errors.items[0]);
        goto done;
    }
    if (strcmp(rdr_inference_conclusion(kb, inf), conclusion) == 0) {
        r->already_correct++;
        goto done;
    }

    /* Correction */
    res = rdr_correct(kb, inf, conclusion, NULL, strategy);
    switch (res.status) {
    case RDR_ACQ_EXCEPTION_ADDED:
        r->exceptions_added++;
        break;
    case RDR_ACQ_REVISION_ADDED:
        r->revisions_added++;
        break;
    case RDR_ACQ_CONCLUSION_EDITED:
        r->conclusions_edited++;
        break;
    case RDR_ACQ_UNCHANGED:
        r->already_correct++;
        break;
    case RDR_ACQ_REJECTED:
        r->rejected++;
        break;
    case RDR_ACQ_ERROR:
        r->failed++;
        break;
    }
    if (res.status != RDR_ACQ_UNCHANGED) {
        strlist_pushf(&r->messages, "line %lu: %s", (unsigned long)line, res.message);
    }

done:
    free(conclusion);
    clauselist_free(&query);
    strlist_free(&errors);
}

int kb_csv_import_text(RdrKb *kb, const char *text, size_t len, RdrRevisionStrategy strategy,
                       CsvImportReport *report, char *err, size_t errlen)
{
    CsvTable table;
    RdrInference inf;
    size_t i;

    csv_report_free(report);
    csv_table_init(&table);
    if (!csv_parse(text, len, &table, err, errlen)) {
        return 0;
    }
    if (table.count == 0 || !header_matches(&table.rows[0])) {
        str_set_error(err, errlen, "the first row must be the header 'case,conclusion'");
        csv_table_free(&table);
        return 0;
    }

    rdr_inference_init(&inf);
    for (i = 1; i < table.count; i++) {
        report->rows++;
        import_row(kb, &table.rows[i], table.lines[i], strategy, report, &inf);
    }
    rdr_inference_free(&inf);
    csv_table_free(&table);
    return 1;
}

int kb_csv_import_file(RdrKb *kb, const char *path, RdrRevisionStrategy strategy,
                       CsvImportReport *report, char *err, size_t errlen)
{
    char *text = NULL;
    size_t len = 0;
    int ok;

    if (!platform_read_file(path, &text, &len, err, errlen)) {
        csv_report_free(report);
        return 0;
    }
    ok = kb_csv_import_text(kb, text, len, strategy, report, err, errlen);
    free(text);
    return ok;
}
