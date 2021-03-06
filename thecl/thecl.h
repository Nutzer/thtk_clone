/*
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain this list
 *    of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce this
 *    list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#ifndef THECL_H_
#define THECL_H_

#include <config.h>
#include <stdbool.h>
#include <stdio.h>
#include "list.h"
#include "value.h"
#include "eclmap.h"
#include "eclarg.h"
#include "util.h"

typedef enum {
    THECL_INSTR_INSTR,
    THECL_INSTR_TIME,
    THECL_INSTR_RANK,
    THECL_INSTR_LABEL
} thecl_instr_type;

#define RANK_EASY      (1 << 0)
#define RANK_NORMAL    (1 << 1)
#define RANK_HARD      (1 << 2)
#define RANK_LUNATIC   (1 << 3)
#define RANK_EXTRA     (1 << 4)
#define RANK_OVERDRIVE (1 << 5)

// Used to describe unused ranks
#define RANK_ID_4      (1 << 4)
#define RANK_ID_5      (1 << 5)
#define RANK_ID_6      (1 << 6)
#define RANK_ID_7      (1 << 7)

typedef struct thecl_param_t {
    int type;
    value_t value;
    int stack;
    char is_expression_param; // Temporary variable for ecsparse.y
} thecl_param_t;

thecl_param_t* param_new(
    int type);
void param_free(
    thecl_param_t* param);

typedef struct thecl_instr_t {
    thecl_instr_type type;
    char* string;

    /* THECL_INSTR_INSTR: */
    unsigned int id;
    size_t param_count;
    list_t params;
    int op_type;
    int size;

    /* Etc.: */
    unsigned int time;
    unsigned int rank;
    unsigned int offset;
} thecl_instr_t;

thecl_instr_t* thecl_instr_new(
    void);

thecl_instr_t* thecl_instr_time(
    unsigned int time);

thecl_instr_t* thecl_instr_rank(
    unsigned int rank);

thecl_instr_t* thecl_instr_label(
    unsigned int offset);

void thecl_instr_free(
    thecl_instr_t* instr);

typedef struct {
    int32_t offset;
    char name[];
} thecl_label_t;

/* TODO: Move label creation functions here. */

/* TODO: Clean this up. */
typedef struct {
    char* name;
    list_t instrs;
    size_t stack;
    ssize_t arity;
    size_t var_count;
    char** vars;
    uint32_t offset;
    list_t labels;
} thecl_sub_t;

/* TODO: Subroutine creation and deletion functions. */

typedef struct {
    char name[256];
    size_t data_length;
    unsigned char data[];
} thecl_local_data_t;

typedef struct {
    thecl_param_t *param;
    char name[256];
} global_definition_t;

/* TODO: Local data deletion and creation functions. */

typedef struct {
    unsigned int version;
    /* TODO: Make local data. */
    size_t anim_count;
    char** anim_names;

    size_t ecli_count;
    char** ecli_names;

    size_t sub_count;
    list_t subs;

    list_t local_data;
} thecl_t;

thecl_t* thecl_new(
    void);

typedef struct {
    thecl_t* (*open)(FILE* stream, unsigned int ver);
    /* Translates the data to a more general format. */
    /* TODO: Return it instead. */
    void (*trans)(thecl_t* ecl);
    void (*dump)(const thecl_t* ecl, FILE* stream);

    thecl_t* (*parse)(FILE* stream, unsigned int ver);
    int (*compile)(const thecl_t* ecl, FILE* stream);
} thecl_module_t;

typedef struct {
    int instr_time;
    int instr_rank/* = 0xff*/;
    unsigned int version;
    bool uses_numbered_subs;
    bool has_overdrive_difficulty;
    bool uses_stack_offsets;
    list_t expressions;
    list_t global_definitions;
    list_t block_stack;
    thecl_sub_t* current_sub;
    thecl_t* ecl;
    const char* (*instr_format)(unsigned int version, unsigned int id);
    size_t (*instr_size)(unsigned int version, const thecl_instr_t* instr);
} parser_state_t;

/* TODO: Deletion and creation functions for parser state. */

extern FILE* yyin;
extern int yyparse(parser_state_t*);

extern eclmap_t* g_eclmap_opcode;
extern eclmap_t* g_eclmap_global;
extern eclarg_includes_t* g_eclarg_includes;
extern bool g_ecl_rawoutput;
extern list_t *custom_fmts;

#endif
