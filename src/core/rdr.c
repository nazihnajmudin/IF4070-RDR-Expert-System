/**
 * @file rdr.c
 * @brief Implementasi engine binary SCRDR.
 */

#include "rdr/rdr.h"

#include <stdlib.h>
#include <string.h>

#include "rdr/factbase.h"

/** Batas atas id node agar indeks tidak membengkak akibat file yang rusak. */
#define RDR_MAX_NODE_ID 1000000

/* ===================================================================== */
/* Node Lifecycle                                                        */
/* ===================================================================== */

static RdrNode *node_new(int id, const char *conclusion)
{
    RdrNode *n = xcalloc(1, sizeof(RdrNode));

    n->id = id;
    n->conclusion = str_trim_dup(conclusion);
    n->revises = RDR_NO_REVISION;
    clauselist_init(&n->rule);
    clauselist_init(&n->cornerstone);
    return n;
}

static void node_free(RdrNode *n)
{
    clauselist_free(&n->rule);
    clauselist_free(&n->cornerstone);
    free(n->conclusion);
    free(n);
}

/* Membebaskan subtree secara iteratif dan menghapus entri indeksnya. */
static size_t free_subtree(RdrKb *kb, RdrNode *top)
{
    RdrNode **stack;
    size_t sp = 0;
    size_t cap = 64;
    size_t removed = 0;

    if (top == NULL) {
        return 0;
    }
    stack = xmalloc(cap * sizeof(RdrNode *));
    stack[sp++] = top;
    while (sp > 0) {
        RdrNode *n = stack[--sp];
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(RdrNode *));
        }
        if (n->true_child != NULL) {
            stack[sp++] = n->true_child;
        }
        if (n->false_child != NULL) {
            stack[sp++] = n->false_child;
        }
        if (kb != NULL && (size_t)n->id < kb->index_cap) {
            kb->index[n->id] = NULL;
        }
        node_free(n);
        removed++;
    }
    free(stack);
    return removed;
}

static void index_put(RdrKb *kb, RdrNode *n)
{
    if ((size_t)n->id >= kb->index_cap) {
        size_t cap = kb->index_cap ? kb->index_cap : 64;
        while (cap <= (size_t)n->id) {
            cap *= 2;
        }
        kb->index = xrealloc(kb->index, cap * sizeof(RdrNode *));
        memset(kb->index + kb->index_cap, 0, (cap - kb->index_cap) * sizeof(RdrNode *));
        kb->index_cap = cap;
    }
    kb->index[n->id] = n;
}

/* ===================================================================== */
/* Knowledge Base                                                        */
/* ===================================================================== */

RdrKb *rdr_kb_create(const char *default_conclusion)
{
    RdrKb *kb;

    if (default_conclusion == NULL || str_is_blank(default_conclusion)) {
        return NULL;
    }
    kb = xcalloc(1, sizeof(RdrKb));
    kb->root = node_new(0, default_conclusion);
    kb->next_id = 1;
    kb->node_count = 1;
    index_put(kb, kb->root);
    return kb;
}

void rdr_kb_destroy(RdrKb *kb)
{
    if (kb == NULL) {
        return;
    }
    free_subtree(NULL, kb->root);
    free(kb->index);
    free(kb);
}

RdrNode *rdr_kb_find(const RdrKb *kb, int id)
{
    if (kb == NULL || id < 0 || (size_t)id >= kb->index_cap) {
        return NULL;
    }
    return kb->index[id];
}

RdrNode *rdr_kb_insert(RdrKb *kb, RdrNode *parent, RdrSide side, int id,
                       const ClauseList *rule, const char *conclusion,
                       const ClauseList *cornerstone, int revises,
                       char *err, size_t errlen)
{
    RdrNode **slot;
    RdrNode *n;

    /* Argument Validation */
    if (parent == NULL) {
        str_set_error(err, errlen, "parent node does not exist");
        return NULL;
    }
    slot = (side == RDR_SIDE_TRUE) ? &parent->true_child : &parent->false_child;
    if (*slot != NULL) {
        str_set_error(err, errlen, "the %s slot of node #%d is already occupied",
                      side == RDR_SIDE_TRUE ? "true" : "false", parent->id);
        return NULL;
    }
    if (conclusion == NULL || str_is_blank(conclusion)) {
        str_set_error(err, errlen, "conclusion cannot be empty");
        return NULL;
    }
    if (id < 0) {
        id = kb->next_id;
    }
    if (id > RDR_MAX_NODE_ID) {
        str_set_error(err, errlen, "node id %d exceeds the maximum of %d", id, RDR_MAX_NODE_ID);
        return NULL;
    }
    if (rdr_kb_find(kb, id) != NULL) {
        str_set_error(err, errlen, "node id %d is already used", id);
        return NULL;
    }

    /* Node Creation */
    n = node_new(id, conclusion);
    if (rule != NULL) {
        clauselist_copy(&n->rule, rule);
    }
    if (cornerstone != NULL) {
        clauselist_copy(&n->cornerstone, cornerstone);
    }
    n->revises = revises;
    n->parent = parent;
    *slot = n;

    /* Index Update */
    index_put(kb, n);
    kb->node_count++;
    if (id >= kb->next_id) {
        kb->next_id = id + 1;
    }
    kb->stamp++;
    return n;
}

int rdr_kb_foreach(const RdrKb *kb, RdrVisitFn visit, void *user)
{
    typedef struct {
        const RdrNode *node;
        int depth;
    } Frame;
    Frame *stack;
    size_t sp = 0;
    size_t cap = 64;
    int result = 0;

    if (kb == NULL || kb->root == NULL) {
        return 0;
    }
    stack = xmalloc(cap * sizeof(Frame));
    stack[sp].node = kb->root;
    stack[sp].depth = 0;
    sp++;
    while (sp > 0 && result == 0) {
        Frame f = stack[--sp];
        result = visit(f.node, f.depth, user);
        if (sp + 2 > cap) {
            cap *= 2;
            stack = xrealloc(stack, cap * sizeof(Frame));
        }
        if (f.node->false_child != NULL) {  /* false didorong dulu agar true diproses lebih awal */
            stack[sp].node = f.node->false_child;
            stack[sp].depth = f.depth + 1;
            sp++;
        }
        if (f.node->true_child != NULL) {
            stack[sp].node = f.node->true_child;
            stack[sp].depth = f.depth + 1;
            sp++;
        }
    }
    free(stack);
    return result;
}

/* ===================================================================== */
/* Inference                                                             */
/* ===================================================================== */

void rdr_inference_init(RdrInference *inf)
{
    memset(inf, 0, sizeof(*inf));
    clauselist_init(&inf->query);
    intlist_init(&inf->trace);
    inf->last_evaluated = -1;
    inf->last_concluded = -1;
}

void rdr_inference_free(RdrInference *inf)
{
    clauselist_free(&inf->query);
    intlist_free(&inf->trace);
    free(inf->steps);
    rdr_inference_init(inf);
}

static void push_step(RdrInference *inf, int node_id, int fired)
{
    if (inf->step_count == inf->step_cap) {
        inf->step_cap = inf->step_cap ? inf->step_cap * 2 : 16;
        inf->steps = xrealloc(inf->steps, inf->step_cap * sizeof(RdrStep));
    }
    inf->steps[inf->step_count].node_id = node_id;
    inf->steps[inf->step_count].fired = fired;
    inf->step_count++;
}

int rdr_infer(const RdrKb *kb, const ClauseList *query, RdrInference *out, StrList *errors)
{
    FactBase *fb;
    const RdrNode *cur;

    rdr_inference_free(out);

    /* Fact Preparation */
    fb = factbase_create(query, errors);
    if (!factbase_is_consistent(fb)) {
        factbase_destroy(fb);
        return 0;
    }

    /* Tree Traversal */
    clauselist_copy(&out->query, query);
    out->last_evaluated = kb->root->id;
    out->last_concluded = kb->root->id;
    intlist_push(&out->trace, kb->root->id);
    push_step(out, kb->root->id, 1);
    cur = kb->root->true_child;
    while (cur != NULL) {
        int fired = factbase_satisfies_all(fb, &cur->rule);
        out->last_evaluated = cur->id;
        push_step(out, cur->id, fired);
        if (fired) {
            out->last_concluded = cur->id;
            intlist_push(&out->trace, cur->id);
            cur = cur->true_child;
        } else {
            cur = cur->false_child;
        }
    }

    factbase_destroy(fb);
    out->stamp = kb->stamp;
    out->valid = 1;
    return 1;
}

RdrNodeStatus rdr_inference_status(const RdrInference *inf, int node_id)
{
    size_t i;

    if (!inf->valid) {
        return RDR_NODE_IDLE;
    }
    for (i = 0; i < inf->step_count; i++) {
        if (inf->steps[i].node_id == node_id) {
            return inf->steps[i].fired ? RDR_NODE_FIRED : RDR_NODE_FAILED;
        }
    }
    return RDR_NODE_IDLE;
}

int rdr_inference_is_current(const RdrKb *kb, const RdrInference *inf)
{
    return kb != NULL && inf->valid && inf->stamp == kb->stamp;
}

const char *rdr_inference_conclusion(const RdrKb *kb, const RdrInference *inf)
{
    const RdrNode *n;

    if (!inf->valid) {
        return NULL;
    }
    n = rdr_kb_find(kb, inf->last_concluded);
    return n ? n->conclusion : NULL;
}

/* ===================================================================== */
/* Knowledge Acquisition                                                 */
/* ===================================================================== */

void rdr_difference(const ClauseList *query, const ClauseList *cornerstone, ClauseList *out)
{
    FactBase *fb = factbase_create(cornerstone, NULL);
    size_t i;

    clauselist_clear(out);
    for (i = 0; i < query->count; i++) {
        if (!factbase_satisfies(fb, &query->items[i])) {
            clauselist_push_copy(out, &query->items[i]);
        }
    }
    factbase_destroy(fb);
}

int rdr_inference_difference(const RdrKb *kb, const RdrInference *inf, ClauseList *out)
{
    const RdrNode *concluded = inf->valid ? rdr_kb_find(kb, inf->last_concluded) : NULL;

    clauselist_clear(out);
    if (concluded == NULL) {
        return 0;
    }
    rdr_difference(&inf->query, &concluded->cornerstone, out);
    return 1;
}

int rdr_validate_exception_rule(const ClauseList *query, const ClauseList *cornerstone,
                                const ClauseList *rule, char *err, size_t errlen)
{
    if (rule->count == 0) {
        str_set_error(err, errlen, "the exception rule needs at least one clause");
        return 0;
    }
    if (!factbase_list_satisfies(query, rule)) {
        str_set_error(err, errlen, "the rule must be satisfied by the current query");
        return 0;
    }
    if (factbase_list_satisfies(cornerstone, rule)) {
        str_set_error(err, errlen,
                      "the rule must not be satisfied by the cornerstone case, otherwise it cannot tell the two cases apart");
        return 0;
    }
    return 1;
}

static RdrAcqResult acq_result(RdrAcqStatus status, int node_id)
{
    RdrAcqResult r;

    r.status = status;
    r.node_id = node_id;
    r.message[0] = '\0';
    return r;
}

RdrAcqResult rdr_correct(RdrKb *kb, const RdrInference *inf, const char *conclusion,
                         const ClauseList *custom_rule, RdrRevisionStrategy strategy)
{
    RdrAcqResult r = acq_result(RDR_ACQ_ERROR, -1);
    RdrNode *concluded;
    RdrNode *evaluated;
    RdrSide side;
    ClauseList diff;
    char *wanted;
    char err[256];

    /* Input Validation */
    if (!inf->valid) {
        str_copy(r.message, sizeof(r.message), "There is no result to correct. Run a query first.");
        return r;
    }
    if (!rdr_inference_is_current(kb, inf)) {
        str_copy(r.message, sizeof(r.message),
                 "The knowledge base changed after the last inference. Run the query again first.");
        return r;
    }
    if (conclusion == NULL || str_is_blank(conclusion)) {
        str_copy(r.message, sizeof(r.message), "The correct conclusion cannot be empty.");
        return r;
    }
    concluded = rdr_kb_find(kb, inf->last_concluded);
    evaluated = rdr_kb_find(kb, inf->last_evaluated);
    if (concluded == NULL || evaluated == NULL) {
        str_copy(r.message, sizeof(r.message), "The inference result refers to a missing node.");
        return r;
    }
    wanted = str_trim_dup(conclusion);
    if (strcmp(wanted, concluded->conclusion) == 0) {
        r.status = RDR_ACQ_UNCHANGED;
        r.node_id = concluded->id;
        str_set_error(r.message, sizeof(r.message),
                      "The conclusion is already '%s'. Nothing was changed.", wanted);
        free(wanted);
        return r;
    }

    side = (evaluated == concluded) ? RDR_SIDE_TRUE : RDR_SIDE_FALSE;
    clauselist_init(&diff);
    rdr_difference(&inf->query, &concluded->cornerstone, &diff);

    if (diff.count > 0) {
        /* Exception Rule */
        const ClauseList *rule = (custom_rule != NULL) ? custom_rule : &diff;
        RdrNode *n;
        if (!rdr_validate_exception_rule(&inf->query, &concluded->cornerstone, rule, err, sizeof(err))) {
            str_set_error(r.message, sizeof(r.message), "Invalid exception rule: %s.", err);
        } else {
            n = rdr_kb_insert(kb, evaluated, side, -1, rule, wanted, &inf->query,
                              RDR_NO_REVISION, err, sizeof(err));
            if (n == NULL) {
                str_set_error(r.message, sizeof(r.message), "Cannot add the exception: %s.", err);
            } else {
                char *rule_text = clauselist_rule_text(&n->rule);
                r = acq_result(RDR_ACQ_EXCEPTION_ADDED, n->id);
                str_set_error(r.message, sizeof(r.message),
                              "Added node #%d (%s >> %s) as the %s child of node #%d.",
                              n->id, rule_text, n->conclusion,
                              side == RDR_SIDE_TRUE ? "true" : "false", evaluated->id);
                free(rule_text);
            }
        }
    } else {
        /* Revision Case */
        switch (strategy) {
        case RDR_REVISION_NODE: {
            RdrNode *n = rdr_kb_insert(kb, evaluated, side, -1, NULL, wanted, &inf->query,
                                       concluded->id, err, sizeof(err));
            if (n == NULL) {
                str_set_error(r.message, sizeof(r.message), "Cannot add the revision: %s.", err);
                break;
            }
            r = acq_result(RDR_ACQ_REVISION_ADDED, n->id);
            str_set_error(r.message, sizeof(r.message),
                          "No clause distinguishes this case from the cornerstone of node #%d. "
                          "Added revision node #%d (T >> %s) as the %s child of node #%d.",
                          concluded->id, n->id, n->conclusion,
                          side == RDR_SIDE_TRUE ? "true" : "false", evaluated->id);
            break;
        }
        case RDR_REVISION_EDIT_CONCLUSION:
            free(concluded->conclusion);
            concluded->conclusion = str_dup(wanted);
            if (concluded != kb->root) {
                clauselist_copy(&concluded->cornerstone, &inf->query);
            }
            kb->stamp++;
            r = acq_result(RDR_ACQ_CONCLUSION_EDITED, concluded->id);
            str_set_error(r.message, sizeof(r.message),
                          "No clause distinguishes this case from the cornerstone of node #%d. "
                          "Changed its conclusion to '%s'.", concluded->id, wanted);
            break;
        case RDR_REVISION_REJECT:
            r = acq_result(RDR_ACQ_REJECTED, -1);
            str_set_error(r.message, sizeof(r.message),
                          "No clause distinguishes this case from the cornerstone of node #%d. "
                          "Add a distinguishing fact to the query and try again.", concluded->id);
            break;
        }
    }

    clauselist_free(&diff);
    free(wanted);
    return r;
}

/* ===================================================================== */
/* Manual Editing                                                        */
/* ===================================================================== */

static RdrNode *require_node(RdrKb *kb, int id, char *err, size_t errlen)
{
    RdrNode *n = rdr_kb_find(kb, id);

    if (n == NULL) {
        str_set_error(err, errlen, "node #%d does not exist", id);
    }
    return n;
}

int rdr_node_set_rule(RdrKb *kb, int id, const ClauseList *rule, char *err, size_t errlen)
{
    RdrNode *n = require_node(kb, id, err, errlen);

    if (n == NULL) {
        return 0;
    }
    if (n == kb->root && rule->count > 0) {
        str_set_error(err, errlen, "the root rule is always T and cannot be changed");
        return 0;
    }
    clauselist_copy(&n->rule, rule);
    kb->stamp++;
    return 1;
}

int rdr_node_set_conclusion(RdrKb *kb, int id, const char *conclusion, char *err, size_t errlen)
{
    RdrNode *n = require_node(kb, id, err, errlen);

    if (n == NULL) {
        return 0;
    }
    if (conclusion == NULL || str_is_blank(conclusion)) {
        str_set_error(err, errlen, "conclusion cannot be empty");
        return 0;
    }
    free(n->conclusion);
    n->conclusion = str_trim_dup(conclusion);
    kb->stamp++;
    return 1;
}

int rdr_node_set_cornerstone(RdrKb *kb, int id, const ClauseList *cornerstone,
                             char *err, size_t errlen)
{
    RdrNode *n = require_node(kb, id, err, errlen);
    StrList errors;

    if (n == NULL) {
        return 0;
    }
    if (n == kb->root && cornerstone->count > 0) {
        str_set_error(err, errlen, "the root cornerstone is always empty");
        return 0;
    }
    strlist_init(&errors);
    if (!factbase_check(cornerstone, &errors)) {
        str_set_error(err, errlen, "%s", errors.items[0]);
        strlist_free(&errors);
        return 0;
    }
    strlist_free(&errors);
    clauselist_copy(&n->cornerstone, cornerstone);
    kb->stamp++;
    return 1;
}

int rdr_node_add_child(RdrKb *kb, int parent_id, RdrSide side, const ClauseList *rule,
                       const char *conclusion, const ClauseList *cornerstone,
                       int *new_id, char *err, size_t errlen)
{
    RdrNode *parent = require_node(kb, parent_id, err, errlen);
    RdrNode *n;

    if (parent == NULL) {
        return 0;
    }
    if (parent == kb->root && side == RDR_SIDE_FALSE) {
        str_set_error(err, errlen, "the false slot of the root is never evaluated because the root is always true");
        return 0;
    }
    if (cornerstone != NULL && !factbase_check(cornerstone, NULL)) {
        str_set_error(err, errlen, "the cornerstone case contains contradicting facts");
        return 0;
    }
    n = rdr_kb_insert(kb, parent, side, -1, rule, conclusion, cornerstone, RDR_NO_REVISION,
                      err, errlen);
    if (n == NULL) {
        return 0;
    }
    if (new_id != NULL) {
        *new_id = n->id;
    }
    return 1;
}

int rdr_node_delete(RdrKb *kb, int id, size_t *removed, char *err, size_t errlen)
{
    RdrNode *n = require_node(kb, id, err, errlen);
    size_t count;

    if (n == NULL) {
        return 0;
    }
    if (n == kb->root) {
        str_set_error(err, errlen, "the root node cannot be deleted");
        return 0;
    }
    if (n->parent->true_child == n) {
        n->parent->true_child = NULL;
    } else {
        n->parent->false_child = NULL;
    }
    count = free_subtree(kb, n);
    kb->node_count -= count;
    kb->stamp++;
    if (removed != NULL) {
        *removed = count;
    }
    return 1;
}

/* ===================================================================== */
/* Validation                                                            */
/* ===================================================================== */

static int collect_id(const RdrNode *node, int depth, void *user)
{
    (void)depth;
    intlist_push((IntList *)user, node->id);
    return 0;
}

static void add_issue(RdrValidation *v, RdrIssueKind kind, int node_id, int reached_id, char *message)
{
    if (v->count == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->items = xrealloc(v->items, v->cap * sizeof(RdrIssue));
    }
    v->items[v->count].kind = kind;
    v->items[v->count].node_id = node_id;
    v->items[v->count].reached_id = reached_id;
    v->items[v->count].message = message;
    v->count++;
    if (kind == RDR_ISSUE_MISMATCH || kind == RDR_ISSUE_INCONSISTENT_CASE) {
        v->errors++;
    }
}

/* Memeriksa apakah node reached merupakan revisi (langsung atau berantai) dari target. */
static int is_revision_of(const RdrKb *kb, const RdrNode *reached, int target_id)
{
    size_t guard = 0;

    while (reached != NULL && reached->revises != RDR_NO_REVISION && guard++ < kb->node_count) {
        if (reached->revises == target_id) {
            return 1;
        }
        reached = rdr_kb_find(kb, reached->revises);
    }
    return 0;
}

void rdr_validate(const RdrKb *kb, RdrValidation *out)
{
    IntList ids;
    RdrInference inf;
    size_t i;

    memset(out, 0, sizeof(*out));
    intlist_init(&ids);
    rdr_inference_init(&inf);
    rdr_kb_foreach(kb, collect_id, &ids);

    for (i = 0; i < ids.count; i++) {
        const RdrNode *n = rdr_kb_find(kb, ids.items[i]);
        const RdrNode *reached;

        if (n == kb->root) {
            continue;
        }
        out->checked++;

        /* Cornerstone Presence */
        if (n->cornerstone.count == 0) {
            add_issue(out, RDR_ISSUE_NO_CORNERSTONE, n->id, -1,
                      str_printf("Node #%d has no cornerstone case.", n->id));
            continue;
        }

        /* Cornerstone Inference */
        if (!rdr_infer(kb, &n->cornerstone, &inf, NULL)) {
            add_issue(out, RDR_ISSUE_INCONSISTENT_CASE, n->id, -1,
                      str_printf("The cornerstone of node #%d contains contradicting facts.", n->id));
            continue;
        }
        reached = rdr_kb_find(kb, inf.last_concluded);
        if (reached == n || strcmp(reached->conclusion, n->conclusion) == 0) {
            continue;
        }
        if (is_revision_of(kb, reached, n->id)) {
            add_issue(out, RDR_ISSUE_SUPERSEDED, n->id, reached->id,
                      str_printf("The cornerstone of node #%d is now concluded as '%s' by revision node #%d.",
                                 n->id, reached->conclusion, reached->id));
            continue;
        }
        add_issue(out, RDR_ISSUE_MISMATCH, n->id, reached->id,
                  str_printf("The cornerstone of node #%d now concludes '%s' at node #%d instead of '%s'.",
                             n->id, reached->conclusion, reached->id, n->conclusion));
    }

    rdr_inference_free(&inf);
    intlist_free(&ids);
}

void rdr_validation_free(RdrValidation *v)
{
    size_t i;

    for (i = 0; i < v->count; i++) {
        free(v->items[i].message);
    }
    free(v->items);
    memset(v, 0, sizeof(*v));
}
