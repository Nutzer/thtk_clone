#include "thanm.h"
#include <stdbool.h>
#include <stdio.h>
#include "value.h"
#include "eclmap.h"
#include "eclarg.h"
#include "util.h"
#include "string.h"
#include "stdlib.h"


typedef enum {
    THECL_INSTR_INSTR,
    THECL_INSTR_TIME,
    THECL_INSTR_RANK,
    THECL_INSTR_LABEL
} thecl_instr_type;

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
  /* AKA Script */
    char* name;
    uint32_t offset;
    list_t instrs;
    list_t labels;
} anm_sub_t;


typedef struct {
    anm_entry_t *entry;
    char *sub_name;
    int sub_id;
} sub_entry_map_t;


typedef struct {
    thecl_param_t *param;
    char name[256];
} global_definition_t;
typedef struct {
    int instr_time;
    int instr_rank/* = 0xff*/;
    unsigned int version;
    bool uses_numbered_subs;
    list_t global_definitions;

    list_t scripts;
    size_t script_count;

    list_t sub_map;

    int current_sprite_id;
    char *current_sprite_name;
    size_t sprite_count;

    anm_entry_t *current_entry;
    size_t entry_count;
    anm_sub_t *current_sub;
    anm_archive_t* anm;
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


/* Main Section end */


typedef struct {
PACK_BEGIN
    char magic[4];
    uint16_t unknown1; /* 1 */
    uint16_t include_length; /* include_offset + ANIM+ECLI length */
    uint32_t include_offset; /* sizeof(th10_header_t) */
    uint32_t zero1;
    uint32_t sub_count;
    uint32_t zero2[4];
PACK_END
} PACK_ATTRIBUTE th10_header_t;

typedef struct {
PACK_BEGIN
    char magic[4];
    uint32_t count;
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th10_list_t;

typedef struct {
PACK_BEGIN
    char magic[4];
    uint32_t data_offset; /* sizeof(th10_sub_t) */
    uint32_t zero[2];
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th10_sub_t;

typedef struct {
PACK_BEGIN
  uint16_t id;
	uint16_t size;
	int16_t time;
	uint16_t param_mask;
	unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th10_instr_t;


anm_archive_t *anm_parse(FILE *in, unsigned int version);


/* FMTS moved here */

static const id_format_pair_t formats_v0[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "ff" },
    { 3, "S" },
    { 4, "S" },
    { 5, "S" },
    { 7, "" },
    { 9, "fff" },
    { 10, "fSf" },
    { 11, "ff" },
    { 12, "SS" },
    { 13, "" },
    { 14, "" },
    { 15, "" },
    { 16, "SS" },
    { 17, "fff" },
    { 18, "ffSS" },
    { 19, "ffSS" },
    { 20, "fffS" },
    { 21, "" },
    { 22, "S" },
    { 23, "" },
    { 24, "" },
    { 25, "S" },
    { 26, "S" },
    { 27, "f" },
    { 28, "f" },
    { 29, "S" },
    { 30, "ffS" },
    { 31, "S" },
    { 0, NULL }
};

static const id_format_pair_t formats_v2[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "S" },
    { 4, "oS" },
    { 5, "SSS" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "S" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "ffSS" },
    { 18, "ffSS" },
    { 19, "fffS" },
    { 20, "" },
    { 21, "S" },
    { 22, "" },
    { 23, "" },
    { 24, "S" },
    { 25, "S" },
    { 26, "f" },
    { 27, "f" },
    { 28, "S" },
    { 29, "ffS" },
    { 30, "S" },
    { 31, "S" },
    { 32, "SSffS" },
    { 33, "SSS" },
    { 34, "SSS" },
    { 35, "SSSSf" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 42, "ff" },
    { 50, "fff" },
    { 52, "fff" },
    { 55, "SSS" },
    { 59, "SS" },
    { 60, "ff" },
    { 69, "SSSS" },
    { 79, "S" },
    { 80, "S" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v3[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "S" },
    { 4, "oS" },
    { 5, "SoS" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "SSS" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "ffSS" },
    { 18, "ffSS" },
    { 20, "" },
    { 21, "S" },
    { 22, "" },
    { 23, "" },
    { 24, "S" },
    { 25, "S" },
    { 26, "f" },
    { 27, "f" },
    { 28, "S" },
    { 30, "S" },
    { 31, "S" },
    { 32, "SSfff" },
    { 33, "SSSSS" },
    { 34, "SSS" },
    { 35, "SSSSf" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 40, "ff" },
    { 42, "ff" },
    { 44, "ff" },
    { 49, "SSS" },
    { 50, "fff" },
    { 52, "fff" },
    { 54, "fff" },
    { 55, "SSS" },
    { 56, "fff" },
    { 59, "SS" },
    { 60, "ff" },
    { 69, "SSSS" },
    { 79, "S" },
    { 80, "f" },
    { 81, "f" },
    { 82, "S" },
    { 83, "S" },
    { 85, "S" },
    { 86, "SSSSS" },
    { 87, "SSS" },
    { 89, "" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v4p[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "S" },
    { 4, "oS" },
    { 5, "SoS" },
    { 6, "SS" },
    { 7, "ff" },
    { 8, "SS" },
    { 9, "ff" },
    { 11, "ff" },
    { 13, "ff" },
    { 18, "SSS" },
    { 19, "fff" },
    { 21, "fff" },
    { 22, "SSS" },
    { 23, "fff" },
    { 24, "SSS" },
    { 25, "fff" },
    { 26, "SSS" },
    { 27, "fff" },
    { 30, "SSSS" },
    { 40, "SS" },
    { 42, "ff" },
    { 43, "ff" },
    { 48, "fff" },
    { 49, "fff" },
    { 50, "ff" },
    { 51, "S" },
    { 52, "SSS" },
    { 53, "fff" },
    { 56, "SSfff" },
    { 57, "SSSSS" },
    { 58, "SSS" },
    { 59, "SSfSf" },
    { 60, "SSff" },
    { 61, "" },
    { 63, "" },
    { 64, "S" },
    { 65, "S" },
    { 66, "S" },
    { 67, "S" },
    { 68, "S" },
    { 69, "" },
    { 70, "f" },
    { 71, "f" },
    { 73, "S" },
    { 74, "S" },
    { 75, "S" },
    { 76, "SSS" },
    { 77, "S" },
    { 78, "SSSSS" },
    { 79, "SSS" },
    { 80, "S" },
    { 81, "" },
    { 82, "S" },
    { 83, "" },
    { 84, "S" },
    { 85, "S" },
    { 86, "S" },
    { 87, "S" },
    { 88, "S" },
    { 89, "S" },
    { 90, "S" },
    { 91, "S" },
    { 92, "S" },
    { 93, "SSf" },
    { 94, "SSf" },
    { 95, "S" },
    { 96, "Sff" },
    { 100, "SfffffSffS" },
    { 101, "S" },
    { 102, "SS" },
    { 103, "ff" },
    { 104, "fS" },
    { 105, "fS" },
    { 106, "fS" },
    { 107, "SSff" },
    { 108, "ff" },
    { 110, "ff" },
    { 111, "S" },
    { 112, "S" },
    { 113, "SSf" },
    { 114, "S" },

    /* These are not actually part of v4p. */
    { 130, "S" },
    { 134, "SSfff" },

    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v8[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "" },
    { 4, "" },
    { 5, "S" },
    { 6, "S" },
    { 7, "" },
    { 100, "SS" },
    { 101, "ff" },
    { 102, "SS" },
    { 103, "ff" },
    { 104, "SS" },
    { 105, "ff" },
    { 107, "ff" },
    { 112, "SSS" },
    { 113, "fff" },
    { 115, "fff" },
    { 117, "fff" },
    { 118, "SSS" },
    { 119, "fff" },
    { 120, "SSS" },
    { 121, "fff" },
    { 122, "SS" },
    { 124, "ff" },
    { 125, "ff" },
    { 130, "ffff" },
    { 131, "ffff" },
    { 200, "SS" },
    { 201, "SSS" },
    { 202, "SSSS" },
    { 204, "SSSS" },
    { 300, "S" },
    { 301, "SS" },
    { 302, "S" },
    { 303, "S" },
    { 304, "S" },
    { 305, "S" },
    { 306, "S" },
    { 307, "S" },
    { 308, "" },
    { 311, "S" },
    { 312, "SS" },
    { 313, "S" },
    { 314, "S" },
    { 315, "S" },
    { 316, "" },
    { 317, "" },
    { 400, "fff" },
    { 401, "fff" },
    { 402, "ff" },
    { 403, "S" },
    { 404, "SSS" },
    { 405, "S" },
    { 406, "SSS" },
    { 407, "SSfff" },
    { 408, "SSSSS" },
    { 409, "SSS" },
    { 410, "SSfff" },
    { 412, "SSff" },
    { 413, "SSSSS" },
    { 414, "SSS" },
    { 415, "fff" },
    { 420, "SffSSSSffS" },
    { 421, "S" }, /* ss */
    { 422, "" },
    { 423, "S" },
    { 424, "S" },
    { 425, "f" },
    { 426, "f" },
    { 428, "SSf" },
    { 429, "Sf" },
    { 430, "SSff" },
    { 431, "S" },
    { 432, "S" },
    { 433, "SSff" },
    { 434, "ff" },
    { 436, "ff" },
    { 437, "S" },
    { 438, "S" },
    { 439, "S" },
    { 500, "S" },
    { 501, "S" },
    { 502, "S" },
    { 503, "S" },
    { 504, "S" },
    { 505, "Sff" },
    { 507, "S" },
    { 508, "S" },
    { 509, "" },
    { 600, "S" },
    { 602, "S" },
    { 603, "ff" },
    { 604, "fS" },
    { 605, "fS" },
    { 606, "ff" },
    { 608, "ff" },
    { 609, "S" },
    { 610, "S" },
    { 611, "ffS" },
    { 612, "ff" },
    { 0xffff, "" },
    { 0, NULL }
};
