#include "anmx_parse.h"

static const char *th10_find_format(unsigned int version, unsigned int id) {
    const char *ret = NULL;

    id_format_pair_t *fmt;
    if (custom_fmts)
        list_for_each(custom_fmts, fmt) {
            if (fmt->id == id) {
                ret = fmt->format;
                break;
            }
        }

    switch (version) {
    case 8:
        if (!ret)
            ret = find_format(formats_v8, id);
    case 4:
    case 7:
        if (!ret)
            ret = find_format(formats_v4p, id);
    case 3:
        if (!ret)
            ret = find_format(formats_v3, id);
    case 2:
        if (!ret)
            ret = find_format(formats_v2, id);
    case 0:
        if (!ret)
            ret = find_format(formats_v0, id);
    }
    return ret;
}

static int32_t label_find(anm_sub_t *sub, const char *name) {
    thecl_label_t *label;
    list_for_each(&sub->labels, label) {
        if (strcmp(label->name, name) == 0)
            return label->offset;
    }
    fprintf(stderr, "label not found: %s\n", name);
    return 0;
}

static unsigned char *th10_instr_serialize(unsigned int version, anm_sub_t *sub,
                                           thecl_instr_t *instr) {
    anm_instr_t *ret;
    thecl_param_t *param;

    ret = malloc(instr->size);

    ret->time = instr->time;
    ret->type = instr->id;
    ret->length = instr->size - sizeof(anm_instr_t);
    ret->param_mask = 0;

    unsigned char *param_data = ret->data;
    int param_count = 0;

    const char *expected_format = th10_find_format(version, instr->id);
    if (expected_format == NULL)
        fprintf(stderr,
                "th10_instr_serialize: in sub %s: instruction with id %d is not "
                "known to exist in version %d\n",
                sub->name, instr->id, version);
    else {
        list_for_each(&instr->params, param) {
            if (expected_format[0] == 0)
                fprintf(stderr,
                        "th10_instr_serialize: in sub %s: too many arguments for "
                        "opcode %d\n",
                        sub->name, instr->id);
            if (expected_format[0] != '*')
                expected_format++;
        }
        if (expected_format[0] != '*' && expected_format[0] != 0)
            fprintf(
                stderr,
                "th10_instr_serialize: in sub %s: too few arguments for opcode %d\n",
                sub->name, instr->id);
    }

    list_for_each(&instr->params, param) {
        if (param->stack)
            ret->param_mask |= 1 << param_count;
        ++param_count;
        if (param->type == 'o') {
            /* This calculates the absolute offset from the current instruction. */
            uint32_t label = label_find(sub, param->value.val.z);
            memcpy(param_data, &label, sizeof(uint32_t));
            param_data += sizeof(uint32_t);
        } else if (param->type == 'x' || param->type == 'm') {
            size_t zlen = strlen(param->value.val.z);
            uint32_t padded_length = zlen + (4 - (zlen % 4));
            memcpy(param_data, &padded_length, sizeof(padded_length));
            param_data += sizeof(padded_length);
            memset(param_data, 0, padded_length);
            strncpy((char *)param_data, param->value.val.z, zlen);
            if (param->type == 'x')
                util_xor(param_data, padded_length, 0x77, 7, 16);
            param_data += padded_length;
        } else
            param_data +=
                value_to_data(&param->value, param_data,
                              instr->size - (param_data - (unsigned char *)ret));

        if (param->stack && (version == 13 || version == 14 || version == 143 ||
                             version == 15 || version == 16 || version == 165)) {
            if (param->type == 'D') {
                struct {
                    char from;
                    char to;
                    union {
                        int32_t S;
                        float f;
                    } val;
                } *temp = (void *)param->value.val.m.data;
            }
        }

        param_free(param);
    }
    list_free_nodes(&instr->params);

    return (unsigned char *)ret;
}

static size_t th10_instr_size(unsigned int version,
                              const thecl_instr_t *instr) {
    size_t ret = sizeof(anm_instr_t);
    thecl_param_t *param;

    list_for_each(&instr->params, param) {
        /* XXX: I think 'z' is what will be passed ... */
        if (param->type == 'm' || param->type == 'x') {
            size_t zlen = strlen(param->value.val.z);
            ret += sizeof(uint32_t) + zlen + (4 - (zlen % 4));
        } else if (param->type == 'o') {
            ret += sizeof(uint32_t);
        } else {
            ret += value_size(&param->value);
        }
    }

    return ret;
}

typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};
extern YYLTYPE yylloc;

static void th10_parse_file(
  FILE *in,
  parser_state_t *state
  ) {

    /* Search for #-commands */
    fseek(in, 0, SEEK_END);
    int file_size = ftell(in);
    char *file = malloc(file_size);
    fseek(in, 0, SEEK_SET);
    fread(file, 1, file_size, in);
    fseek(in, 0, SEEK_SET);

    for(int i = 0; i < file_size; i++) {
        /* instr: add custom instruction */
        if(strncmp(file+i, "#instr", 4) == 0) {
            char *result = eclarg_get_str(file, i, file_size);
            id_format_pair_t *fmt = malloc(sizeof(id_format_pair_t));
            fmt->format = malloc(255);
            strcpy(fmt->format, "");
            if(2 < sscanf(result, "%i %s", &fmt->id, (char*)fmt->format)) {
              fprintf(stderr, "malformed instruction %s\n", result);
              exit(1);
            }
            list_append_new(custom_fmts, fmt);
        }
        /* map: add map */
        if(strncmp(file+i, "#map", 4) == 0) {
            char *result = eclarg_get_str(file, i, file_size);
            FILE *map_file = eclarg_get_file(g_eclarg_includes, result, "r");
            if (!map_file) {
                fprintf(stderr, "couldn't open %s for reading\n",
                        result);
                exit(1);
            }
            eclmap_load(g_eclmap_opcode, g_eclmap_global, map_file, result);
            fclose(map_file);
        }

        /* include: include file */
        if(strncmp(file+i, "#include", 8) == 0) {
            char *result = eclarg_get_str(file, i, file_size);
            FILE *inc = eclarg_get_file(g_eclarg_includes, result, "rb");
            if(!inc) {
                fprintf(stderr, "couldn't open %s for writing\n", result);
                fclose(inc);
                exit(1);
            }
            th10_parse_file(inc, state);
            fclose(inc);
        }
    }

    free(file);

    /* Reset parser state */
    yylloc.first_line = 1;
    yylloc.last_line = 1;
    yylloc.first_column = 1;
    yylloc.last_column = 1;

    /* Parse */
    yyin = in;

    if(yyparse(state) != 0) {
        exit(1);
    }

}


anm_archive_t *anm_parse(FILE *in, unsigned int version) {
    parser_state_t state;

    state.instr_time = 0;
    state.instr_rank = 0xff;
    state.version = version;
    state.uses_numbered_subs = false;
    list_init(&state.global_definitions);
    state.current_sub = NULL;
    state.instr_format = th10_find_format;
    state.instr_size = th10_instr_size;

    state.anm = malloc(sizeof(*state.anm));
    state.anm->map = NULL;

    list_init(&state.anm->names);
    list_init(&state.anm->entries);
    list_init(&state.scripts);
    list_init(&state.sub_map);
    state.script_count = 0;
    state.entry_count = 0;
    state.sprite_count = 0;

    th10_parse_file(in, &state);
    /* yyin = in; */
    /* if (yyparse(&state) != 0) { */
    /*     exit(1); */
    /* } */

    float offset = 0;
    sub_entry_map_t *map;
    list_for_each(&state.sub_map, map) {
        anm_sub_t *sub;
        bool matched = false;
        list_for_each(&state.scripts, sub) {
            thecl_instr_t *instr;

            if (strcmp(map->sub_name, sub->name) == 0) {
                anm_script_t *script = malloc(sizeof(*script));
                script->offset = malloc(sizeof(anm_offset_t));
                script->offset->id = map->sub_id;
                script->offset->offset = 0;
                list_init(&script->instrs);

                list_for_each(&sub->instrs, instr) {
                    anm_instr_t *data = th10_instr_serialize(version, sub, instr);
                    list_append_new(&script->instrs, data);
                    free(instr);
                }

                list_append_new(&map->entry->scripts, script);


                matched = true;
                break;
            }
        }
        if (!matched) {
            printf("Warning: No sub match for %s!\n", sub->name);
        }
    }


    anm_sub_t *sub;
    list_for_each(&state.scripts, sub) {
        thecl_label_t *label;
        list_for_each(&sub->labels, label) { free(label); }
        list_free_nodes(&sub->labels);
        list_free_nodes(&sub->instrs);
        free(sub->name);
        free(sub);
    }
    list_free_nodes(&state.scripts);

    list_for_each(&state.sub_map, map) {
        free(map->sub_name);
        free(map);
    }
    list_free_nodes(&state.sub_map);

    global_definition_t *def;
    list_for_each(&state.global_definitions, def) {
        param_free(def->param);
        free(def);
    }
    list_free_nodes(&state.global_definitions);

    return state.anm;
}
