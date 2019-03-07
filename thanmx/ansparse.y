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
%{
#include <config.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expr.h"
#include "file.h"
#include "list.h"
#include "program.h"
#include "anmx_parse.h"
#include "value.h"

typedef struct {
    char* text;
} string_t;

static list_t* string_list_add(list_t* list, char* text);
static void string_list_free(list_t* list);

static thecl_instr_t* instr_new(parser_state_t* state, unsigned int id, const char* format, ...);
static thecl_instr_t* instr_new_list(parser_state_t* state, unsigned int id, list_t* list);
static void instr_add(anm_sub_t* sub, thecl_instr_t* instr);

enum expression_type {
    EXPRESSION_OP,
    EXPRESSION_VAL,
};

typedef struct expression_t {
    /* Operator or value. */
    enum expression_type type;
    int id;
    /* For values: The value. */
    thecl_param_t* value;
    /* For operators: The child expressions. */
    list_t children;
    /* Resulting type of expression. */
    int result_type;
} expression_t;

typedef struct {
  expression_t *expr;
  char labelstr[250];
} switch_case_t;

static int parse_rank(const parser_state_t* state, const char* value);

static expression_t* expression_load_new(const parser_state_t* state, thecl_param_t* value);
static expression_t* expression_operation_new(const parser_state_t* state, const int* symbols, expression_t** operands);
static expression_t* expression_address_operation_new(const parser_state_t* state, const int* symbols, thecl_param_t* value);
static void expression_output(parser_state_t* state, expression_t* expr);
static void expression_free(expression_t* expr);
static expression_t *expression_copy(expression_t *expr);
#define EXPR_22(a, b, A, B) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_12(a, A, B) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_11(a, A) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, NULL })
#define EXPR_1A(a, A) \
    expression_address_operation_new(state, (int[]){ a, 0 }, A)
#define EXPR_1B(a, b, A) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, NULL })

void global_definition_add_S(parser_state_t *state, char *name, int S);
int global_definition_get_S(parser_state_t *state, char *name);
/* Block things. */
static void block_create_goto(parser_state_t *state, int type, char *labelstr);

/* Bison things. */
void yyerror(parser_state_t*, const char*);
int yylex(void);
extern FILE* yyin;

/* Parser APIs. */

/* Starts a new subroutine. */
static void sub_begin(parser_state_t* state, char* name);
/* Closes the current subroutine. */
static void sub_finish(parser_state_t* state);

/* Stores a new label in the current subroutine pointing to the current offset. */
static void label_create(parser_state_t* state, char* label);

/* Update the current time label. */
void set_time(parser_state_t* state, int new_time);

%}

%error-verbose
%locations
%parse-param {parser_state_t* state}

%union {
    /* Values from Flex: */
    int integer;
    float floating;
    char* string;
    struct {
        unsigned int length;
        unsigned char* data;
    } bytes;

    /* Internal types: */
    struct thecl_param_t* param;
    struct expression_t* expression;
    struct list_t* list;
}

%token <integer> INSTRUCTION "instruction"
%token <string> IDENTIFIER "identifier"
%token <string> TEXT "text"
%token <integer> INTEGER "integer"
%token <floating> FLOATING "float"
%token <string> RANK "rank"
%token PREPROC "#"
%token COMMA ","
%token COLON ":"
%token SEMICOLON ";"
%token SQUARE_OPEN "["
%token SQUARE_CLOSE "]"
%token ANIM "anim"
%token ECLI "ecli"
%token SUB "sub"
%token VAR "var"
%token LOCAL "local"
%token GLOBAL "global"
%token AT "@"
%token BRACE_OPEN "{"
%token BRACE_CLOSE "}"
%token PARENTHESIS_OPEN "("
%token PARENTHESIS_CLOSE ")"
%token ILLEGAL_TOKEN "illegal token"
%token END_OF_FILE 0 "end of file"
%token ASSOC "->"

%token EQUAL "="
%token GOTO "goto"
%token IMG "img"
%token SPRITE "sprite"
%token MAP "map"
%token PLUS "+"
%token STAR "*"

%token SIZE "Size"

%token VERSION "Version"
%token NAME "Name"
%token NAME2 "Name2"
%token FORMAT "Format"
%token WIDTH "Width"
%token HEIGHT "Height"
%token X_OFFSET "X-Offset"
%token Y_OFFSET "Y-Offset"
%token COLORKEY "ColorKey"
%token MEMORYPRIORITY "MemoryPriority"
%token LOWRESSCALE "LowResScale"
%token HASDATA "HasData"
%token THTX_SIZE "THTX-Size"
%token THTX_FORMAT "THTX-Format"
%token THTX_WIDTH "THTX-Width"
%token THTX_HEIGHT "THTX-Height"
%token THTX_ZERO "THTX-Zero"


%type <list> Instruction_Parameters_List
%type <list> Instruction_Parameters

%type <param> Instruction_Parameter
%type <param> Address
%type <param> Global_Def
%type <param> Address_Type
%type <param> Integer
%type <param> Floating
%type <param> Text
%type <param> Label

%%

Statements:
    | Statement Statements
    ;

Statement:
      "img" "{" ImgEntry "}" ";"
      | IDENTIFIER ":" "sprite" Text INTEGER "*" INTEGER "+" INTEGER "+" INTEGER ";" {
          int entry_id = global_definition_get_S(state, $1);
          anm_entry_t *entry;
          int it = 0;
          list_for_each(&state->anm->entries, entry) {
              if(it++ == entry_id)
                  break;
          }
          sprite_t *sprite = malloc(sizeof(sprite_t));
          sprite->id = state->sprite_count++;
          sprite->w = $5;
          sprite->h = $7;
          sprite->x = $9;
          sprite->y = $11;
          list_append_new(&entry->sprites, sprite);

          global_definition_add_S(state, $4->value.val.z, sprite->id);
          param_free($4);
      }
      | IDENTIFIER ":" "sprite" Text {
          anm_entry_t *entry;
          int it = 0;
          int entry_id = global_definition_get_S(state, $1);
          list_for_each(&state->anm->entries, entry) {
              if(it++ == entry_id)
                  break;
          }
          state->current_sprite_id = 0;
          state->current_sprite_name = strdup($4->value.val.z);
          state->current_entry = entry;
          param_free($4);

      } "{" Sprite_List "}" ";" {
        free(state->current_sprite_name);
      }
      | IDENTIFIER ":" "map" {
          anm_entry_t *entry;
          int it = 0;
          int entry_id = global_definition_get_S(state, $1);
          list_for_each(&state->anm->entries, entry) {
              if(it++ == entry_id)
                  break;
          }
          state->current_entry = entry;
      } "{" Sub_Map "}" ";"
      | "img" ImgEntry ";"
      | "sub" IDENTIFIER "(" ")" {
          sub_begin(state, $2);
          free($2);
      }
      "{" Subroutine_Body "}" {
        sub_finish(state);
      }
    | "global" "[" IDENTIFIER "]" "=" Global_Def ";" {
        global_definition_t *def = malloc(sizeof(global_definition_t));
        strncpy(def->name, $3, 256);
        def->param = $6;
        list_append_new(&state->global_definitions, def);
      }
    | "#" IDENTIFIER Text { /* Ignore */ }
    | "#" "map" Text { /* Ignore */ }
    ;

Sub_Map:
    | Sub_Map INTEGER ":" IDENTIFIER ";" {
        sub_entry_map_t *map = malloc(sizeof(sub_entry_map_t));
        map->entry = state->current_entry;
        map->sub_id = $2;
        map->sub_name = strdup($4);
        list_append_new(&state->sub_map, map);
    }

Sprite_List:
    INTEGER "*" INTEGER "+" INTEGER "+" INTEGER {
        sprite_t *sprite = malloc(sizeof(sprite_t));
        sprite->id = state->sprite_count++;
        sprite->w = $1;
        sprite->h = $3;
        sprite->x = $5;
        sprite->y = $7;
        list_append_new(&state->current_entry->sprites, sprite);

        char buf[256];
        snprintf(buf, 256, "%s_%i", state->current_sprite_name, state->current_sprite_id++);
        global_definition_add_S(state, buf, sprite->id);
    }
    | Sprite_List "," INTEGER "*" INTEGER "+" INTEGER "+" INTEGER {
        sprite_t *sprite = malloc(sizeof(sprite_t));
        sprite->id = state->sprite_count++;
        sprite->w = $3;
        sprite->h = $5;
        sprite->x = $7;
        sprite->y = $9;
        list_append_new(&state->current_entry->sprites, sprite);

        char buf[256];
        snprintf(buf, 256, "%s_%i", state->current_sprite_name, state->current_sprite_id++);
        global_definition_add_S(state, buf, sprite->id);
      }
    ;

ImgEntry:
    IDENTIFIER ":" Text INTEGER "*" INTEGER {
        anm_entry_t *entry = malloc(sizeof(anm_entry_t));

        entry->header = calloc(1, sizeof(*entry->header));
        entry->thtx = calloc(1, sizeof(*entry->thtx));
        entry->thtx->magic[0] = 'T';
        entry->thtx->magic[1] = 'H';
        entry->thtx->magic[2] = 'T';
        entry->thtx->magic[3] = 'X';
        entry->name = NULL;
        entry->name2 = NULL;
        list_init(&entry->sprites);
        list_init(&entry->scripts);
        entry->data = NULL;
        list_append_new(&state->anm->entries, entry);

        entry->header->version = state->version;

        entry->name = strdup($3->value.val.z);
        list_append_new(&state->anm->names, entry->name);
        entry->name2 = NULL;
        entry->header->format = 11;
        entry->header->w = $4;
        entry->header->h = $6;
        entry->header->x = 0;
        entry->header->y = 0;
        entry->header->colorkey = 0;
        entry->header->zero3 = 0;
        entry->header->memorypriority = 0;
        entry->header->hasdata = 1;
        entry->thtx->format = 11;
        entry->thtx->w = $4;
        entry->thtx->h = $6;
        entry->thtx->size = $4*$6*4; /* Generally */
        entry->thtx->zero = 0;

        global_definition_add_S(state, $1, state->entry_count++);

        state->current_entry = entry;

        param_free($3);
    } ImgEntryOptions {
        state->current_entry = NULL;
    }
    ;

ImgEntryOptions:
    | ImgEntryOptions "," "Size" ":" INTEGER {state->current_entry->thtx->size = $5;}
    | ImgEntryOptions "," "Version" ":" INTEGER {state->current_entry->header->version = $5;}
    | ImgEntryOptions "," "Format" ":" INTEGER {state->current_entry->header->format = $5;}
    | ImgEntryOptions "," "Width" ":" INTEGER {state->current_entry->header->w = $5;}
    | ImgEntryOptions "," "Height" ":" INTEGER {state->current_entry->header->h = $5;}
    | ImgEntryOptions "," "X-Offset" ":" INTEGER {state->current_entry->header->x = $5;}
    | ImgEntryOptions "," "Y-Offset" ":" INTEGER {state->current_entry->header->y = $5;}
    | ImgEntryOptions "," "Zero3" ":" INTEGER {state->current_entry->header->zero3 = $5;}
    | ImgEntryOptions "," "HasData" ":" INTEGER {state->current_entry->header->zero3 = $5;}
    | ImgEntryOptions "," "THTX-Format" ":" INTEGER {state->current_entry->thtx->format = $5;}
    | ImgEntryOptions "," "THTX-Height" ":" INTEGER {state->current_entry->thtx->h = $5;}
    | ImgEntryOptions "," "THTX-Width" ":" INTEGER {state->current_entry->thtx->w = $5;}
    | ImgEntryOptions "," "THTX-Zero" ":" INTEGER {state->current_entry->thtx->zero = $5;}
    | ImgEntryOptions "," "ColorKey" ":" INTEGER {
        if(state->current_entry->header->version >= 7)
            yyerror(state, "ColorKey is no longer supported in ANM versions >= 7\n");
        state->current_entry->header->colorkey = $5;
    }
    | ImgEntryOptions "," "MemoryPriority" ":" INTEGER {
        if(state->current_entry->header->version == 0)
            yyerror(state, "MemoryPriority is ignored in ANM version 0");
        state->current_entry->header->memorypriority = $5;
    }
    | ImgEntryOptions "," "LowResScale" ":" INTEGER {
        if(state->current_entry->header->version < 8)
            yyerror(state, "LowResScale is ignored in ANM versions < 8");
        state->current_entry->header->lowresscale = $5;
    }

    | ImgEntryOptions "," "Name" ":" Text {
        free(state->current_entry->name);
        state->current_entry->name = strdup($5->value.val.z);
        param_free($5);
    }
    | ImgEntryOptions "," "Name2" ":" Text {
        if(state->current_entry->name2)
            free(state->current_entry->name2);
        state->current_entry->name2 = strdup($5->value.val.z);
        param_free($5);
    }
    ;

Subroutine_Body:
    Instructions
    ;

Global_Def:
    Address
    | Integer
    | Floating
    ;

Instructions:
      Instruction ";"
    | INTEGER ":" { set_time(state, $1); }
    | IDENTIFIER ":" { label_create(state, $1); free($1); }
    | Instructions INTEGER ":" { set_time(state, $2); }
    | Instructions IDENTIFIER ":" { label_create(state, $2); free($2); }
    | Instructions Instruction ";"
    | Instructions INTEGER "->" {
      instr_add(state->current_sub, instr_new(state, 63, ""));
      instr_add(state->current_sub, instr_new(state, 64, "S", $2));
      } "{" Instructions "}" {
        /* XXX: This leads to 2 63's between gotos */
        instr_add(state->current_sub, instr_new(state, 63, ""));
      }
    ;

    /* TODO: Check the given parameters against the parameters expected for the
     *       instruction. */
Instruction:
      IDENTIFIER "(" Instruction_Parameters ")" {
        eclmap_entry_t* ent = eclmap_find(g_eclmap_opcode, $1);
        if(!ent) {
            yyerror(state, "unknown mnemonic");
        }
        else {
            instr_add(state->current_sub, instr_new_list(state, ent->opcode, $3));
        }

        free($3);
      }
    | INSTRUCTION "(" Instruction_Parameters ")" {
        instr_add(state->current_sub, instr_new_list(state, $1, $3));
        free($3);
      }
    | "goto" Label "@" Integer {
        /* const expr_t* expr = expr_get_by_symbol(state->version, GOTO); */
        if(state->version == 3 || state->version == 4 || state->version == 7) {
          instr_add(state->current_sub, instr_new(state, 4, "pp", $2, $4));
        } else {
            yyerror(state, "GOTO-Instruction not known");
        }
      }
    ;

Instruction_Parameters:
      {
        $$ = NULL;
      }
    | Instruction_Parameters_List
    ;

Instruction_Parameters_List:
      Instruction_Parameter {
        $$ = list_new();
        list_append_new($$, $1);
      }
    | Instruction_Parameters_List "," Instruction_Parameter {
        $$ = $1;
        list_append_new($$, $3);
      }
    ;

Instruction_Parameter:
      Address
    | Integer
    | Floating
    | Text
    | Label ;

Address:
      "[" Address_Type "]" {
        $$ = $2;
        $$->stack = 1;
      }
    | "[" IDENTIFIER "]" {
        global_definition_t *def;
        bool found = 0;
        list_for_each(&state->global_definitions, def) {
            if(strcmp(def->name, $2) == 0) {
                thecl_param_t *param = malloc(sizeof(thecl_param_t));
                memcpy(param, def->param, sizeof(thecl_param_t));
                $$ = param;
                found = 1;
                break;
            }
        }
        if(!found) {
            fprintf(stderr, "%s:instr_set_types: in sub %s: global definition not found: %s\n",
                    argv0, state->current_sub->name, $2);
            exit(1);
        }
      }
    ;

Address_Type:
      Integer
    | Floating
    ;

Integer:
    INTEGER {
        $$ = param_new('S');
        $$->value.val.S = $1;
      }
    ;

Floating:
    FLOATING {
        $$ = param_new('f');
        $$->value.val.f = $1;
      }
    ;

Text:
    TEXT {
        $$ = param_new('z');
        $$->value.val.z = $1;
      }
    ;

Label:
    IDENTIFIER {
        $$ = param_new('o');
        $$->value.type = 'z';
        $$->value.val.z = $1;
      }
    ;

%%

static list_t*
string_list_add(
    list_t* list,
    char* text)
{
    string_t* s = malloc(sizeof(string_t));
    s->text = text;
    list_append_new(list, s);
    return list;
}

static void
string_list_free(
    list_t* list)
{
    string_t* s;
    list_for_each(list, s) {
        free(s->text);
        free(s);
    }
    list_free_nodes(list);
    free(list);
}

static thecl_instr_t*
instr_init(
    parser_state_t* state)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->time = state->instr_time;
    instr->rank = state->instr_rank;
    return instr;
}

static void
instr_set_types(
    parser_state_t* state,
    thecl_instr_t* instr)
{
    const char* format = state->instr_format(state->version, instr->id);

    thecl_param_t* param;
    list_for_each(&instr->params, param) {
        int new_type;
        /* XXX: How to check for errors?
         * Perhaps some kind of function that returns a list of satisfying types?
         * Or should there only be one type? */
        /* TODO: Implement * and ? if needed. */
        if (*format == '*')
            new_type = *(format + 1);
        else
            new_type = *format;

        if (new_type != param->type &&
            !(param->type == 'z' && (new_type == 'm' || new_type == 'x')) &&
            !(param->type == 'S' && new_type == 's')) {

            fprintf(stderr, "%s:instr_set_types: in sub %s: wrong argument type for opcode %d (expected: %c, got: %c)\n", argv0, state->current_sub->name, instr->id, new_type, param->type);
        }

        param->type = new_type;

        if (*format != '*')
            ++format;
    }

    return;
}

static thecl_instr_t*
instr_new(
    parser_state_t* state,
    unsigned int id,
    const char* format,
    ...)
{
    va_list ap;
    thecl_instr_t* instr = instr_init(state);
    instr->id = id;

    va_start(ap, format);
    while (*format) {
        thecl_param_t* param;
        if (*format == 'p') {
            param = va_arg(ap, thecl_param_t*);
        } else if (*format == 'S') {
            param = param_new('S');
            param->value.val.S = va_arg(ap, int32_t);
        } else {
            param = NULL;
        }
        list_append_new(&instr->params, param);
        ++instr->param_count;
        ++format;
    }
    va_end(ap);

    instr_set_types(state, instr);

    instr->size = state->instr_size(state->version, instr);

    return instr;
}

static thecl_instr_t*
instr_new_list(
    parser_state_t* state,
    unsigned int id,
    list_t* list)
{
    thecl_instr_t* instr = instr_init(state);
    thecl_param_t* param;

    instr->id = id;
    if (list) {
        int param_id = -1;
        list_for_each(list, param) {
            ++instr->param_count;
            list_append_new(&instr->params, param);
        }
        list_free_nodes(list);
    }

    instr_set_types(state, instr);

    instr->size = state->instr_size(state->version, instr);

    return instr;
}

static void
instr_add(
    anm_sub_t* sub,
    thecl_instr_t* instr)
{
    list_append_new(&sub->instrs, instr);
    instr->offset = sub->offset;
    sub->offset += instr->size;
}

static bool
check_rank_flag(
    const parser_state_t* state,
    const char* value,
    char flag)
{
    int count = 0;
    for (int i=0; value[i]; i++) if(value[i] == flag) count++;

    if (count == 0) return false;
    else if(count == 1) return true;
    else {
        fprintf(stderr, "%s:check_rank_flag: in sub %s: duplicate rank flag %c in '%s'\n", argv0, state->current_sub->name, flag, value);
        return true;
    }
}


static void
sub_begin(
    parser_state_t* state,
    char* name)
{
    anm_sub_t* sub = malloc(sizeof(anm_sub_t));

    sub->name = strdup(name);
    list_init(&sub->instrs);
    sub->offset = 0;
    list_init(&sub->labels);

    // Touhou expects the list of subs to be sorted by name.
    anm_sub_t* iter_sub;
    list_for_each(&state->scripts, iter_sub) {
        int diff = strcmp(name, iter_sub->name);
        if(diff == 0) {
            char buf[256];
            snprintf(buf, 256, "duplicate sub: %s", name);
            yyerror(state, buf);
            break;
        } else if(diff < 0) {
            list_prepend_to(&state->scripts, sub, node);
            goto no_append;
        }
    }
    list_append_new(&state->scripts, sub);
no_append:

    ++state->script_count;
    state->instr_time = 0;
    state->current_sub = sub;
}

static void
sub_finish(
    parser_state_t* state)
{
    state->current_sub = NULL;
}

static void
label_create(
    parser_state_t* state,
    char* name)
{
    thecl_label_t* label = malloc(sizeof(thecl_label_t) + strlen(name) + 1);
    list_prepend_new(&state->current_sub->labels, label);
    label->offset = state->current_sub->offset;
    strcpy(label->name, name);
}

void
set_time(
    parser_state_t* state,
    int new_time)
{
    if (new_time == state->instr_time || (state->instr_time > 0 && new_time < state->instr_time)) {
        char buf[256];
        snprintf(buf, 256, "illegal timer change: %d to %d", state->instr_time, new_time);
        yyerror(state, buf);
    }
    state->instr_time = new_time;
}

int global_definition_get_S(parser_state_t *state, char *name) {
  global_definition_t *def;
  list_for_each(&state->global_definitions, def) {
    if(strcmp(def->name, name) == 0) {
      return def->param->value.val.S;
    }
  }
  char buf[256];
  snprintf(buf, 256, "Could not find definition:", name);
  yyerror(state, buf);
  return 0;
}

void global_definition_add_S(parser_state_t *state, char *name, int S) {
  global_definition_t *def = malloc(sizeof(global_definition_t));
  strncpy(def->name, name, 256);
  def->param = param_new('S');
  def->param->value.val.S = S;
  list_append_new(&state->global_definitions, def);
}

thecl_instr_t*
thecl_instr_new(void)
{
    thecl_instr_t* instr = calloc(1, sizeof(thecl_instr_t));
    instr->type = THECL_INSTR_INSTR;
    list_init(&instr->params);
    return instr;
}

thecl_instr_t*
thecl_instr_time(unsigned int time)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_TIME;
    instr->time = time;
    return instr;
}

thecl_instr_t*
thecl_instr_rank(unsigned int rank)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_RANK;
    instr->rank = rank;
    return instr;
}

thecl_instr_t*
thecl_instr_label(unsigned int offset)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_LABEL;
    instr->offset = offset;
    return instr;
}

void
thecl_instr_free(thecl_instr_t* instr)
{
    free(instr->string);

    thecl_param_t* param;
    list_for_each(&instr->params, param) {
        value_free(&param->value);
        free(param);
    }
    list_free_nodes(&instr->params);

    free(instr);
}

thecl_param_t*
param_new(
    int type)
{
    thecl_param_t* param = calloc(1, sizeof(thecl_param_t));
    param->type = type;
    param->value.type = type;
    param->is_expression_param = 0;
    return param;
}

void
param_free(
    thecl_param_t* param)
{
    value_free(&param->value);
    free(param);
}
void
yyerror(
    parser_state_t* state,
    const char* str)
{
    /* TODO: Research standard row and column range formats. */
    if (yylloc.first_line == yylloc.last_line) {
        if (yylloc.first_column == yylloc.last_column) {
            fprintf(stderr,
                    "%s:%s:%d,%d: %s\n",
                    argv0, current_input,
                    yylloc.first_line, yylloc.first_column, str);
        } else {
            fprintf(stderr,
                    "%s:%s:%d,%d-%d: %s\n",
                    argv0, current_input, yylloc.first_line,
                    yylloc.first_column, yylloc.last_column, str);
        }
    } else {
        fprintf(stderr,
                "%s:%s:%d,%d-%d,%d: %s\n",
                argv0, current_input, yylloc.first_line,
                yylloc.first_column, yylloc.last_line, yylloc.last_column, str);
    }
}
