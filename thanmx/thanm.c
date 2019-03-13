#define HAVE_LIBPNG
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
#include <config.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "image.h"
#include "thanm.h"
#include "program.h"
#include "util.h"
#include "value.h"
#include "mygetopt.h"
#include "anmx_parse.h"

eclmap_t* g_eclmap_opcode;
eclmap_t* g_eclmap_global;
eclarg_includes_t* g_eclarg_includes;
bool g_ecl_rawoutput;
list_t *custom_fmts;

unsigned int option_force;

/* The order and sizes of fields changed for TH11. */
static void
convert_header_to_old(
    anm_header06_t* header)
{
    anm_header11_t th11 = *(anm_header11_t*)header;
    header->sprites = th11.sprites;
    header->scripts = th11.scripts;
    header->rt_textureslot = 0;
    header->w = th11.w;
    header->h = th11.h;
    header->format = th11.format;
    header->colorkey = 0;
    header->nameoffset = th11.nameoffset;
    header->x = th11.x;
    header->y = th11.y;
    header->version = th11.version;
    header->memorypriority = th11.memorypriority;
    header->thtxoffset = th11.thtxoffset;
    header->hasdata = th11.hasdata;
    header->lowresscale = th11.lowresscale;
    header->nextoffset = th11.nextoffset;
    header->zero3 = 0;
}

#ifdef HAVE_LIBPNG
static void
convert_header_to_11(
    anm_header06_t* oldheader)
{
    anm_header06_t header = *oldheader;
    anm_header11_t* th11 = (anm_header11_t*)oldheader;
    memset(th11, 0, sizeof(anm_header11_t));
    th11->sprites = header.sprites;
    th11->scripts = header.scripts;
    th11->zero1 = 0;
    th11->w = header.w;
    th11->h = header.h;
    th11->format = header.format;
    th11->nameoffset = header.nameoffset;
    th11->x = header.x;
    th11->y = header.y;
    th11->version = header.version;
    th11->memorypriority = header.memorypriority;
    th11->thtxoffset = header.thtxoffset;
    th11->hasdata = header.hasdata;
    th11->lowresscale = header.lowresscale;
    th11->nextoffset = header.nextoffset;
}
#endif

static char*
anm_get_name(
    anm_archive_t* archive,
    const char* name)
{
    char* other_name;
    list_for_each(&archive->names, other_name) {
        if (strcmp(name, other_name) == 0)
            return other_name;
    }

    other_name = strdup(name);
    list_append_new(&archive->names, other_name);
    return other_name;
}

static anm_archive_t*
anm_read_file(
    FILE* in)
{
    anm_archive_t* archive = malloc(sizeof(*archive));
    list_init(&archive->names);
    list_init(&archive->entries);

    long file_size;
    unsigned char* map_base;
    unsigned char* map;

    archive->map_size = file_size = file_fsize(in);
    archive->map = map_base = file_mmap(in, file_size);
    map = map_base;

    for (;;) {
        anm_entry_t* entry = malloc(sizeof(*entry));
        anm_header06_t* header = (anm_header06_t*)map;

        list_append_new(&archive->entries, entry);

        if(header->rt_textureslot != 0) {
            header = malloc(sizeof(*header));
            memcpy(header, map, sizeof(*header));
            convert_header_to_old(header);
        }

        entry->header = header;

        assert(
            header->version == 0 ||
            header->version == 2 ||
            header->version == 3 ||
            header->version == 4 ||
            header->version == 7 ||
            header->version == 8);

        assert(header->hasdata == 0 || header->hasdata == 1);
        assert(header->rt_textureslot == 0);
        assert(header->zero3 == 0);

        if(header->version == 8)
            assert(header->lowresscale == 0 || header->lowresscale == 1);

        /* Lengths, including padding, observed are: 16, 32, 48. */
        entry->name = anm_get_name(archive, (const char*)map + header->nameoffset);
        if (header->version == 0 && header->y != 0)
            entry->name2 = (char*)map + header->y;
        else
            entry->name2 = NULL;

        assert(
            (header->hasdata == 0 || (entry->name && entry->name[0] == '@')) ==
            (header->thtxoffset == 0));

        list_init(&entry->sprites);
        if (header->sprites) {
            uint32_t* sprite_offsets = (uint32_t*)(map + sizeof(*header));
            for (uint32_t s = 0; s < header->sprites; ++s) {
                list_append_new(&entry->sprites, (sprite_t*)(map + sprite_offsets[s]));
            }
        }

        list_init(&entry->scripts);
        if (header->scripts) {
            anm_offset_t* script_offsets =
                (anm_offset_t*)(map + sizeof(*header) + header->sprites * sizeof(uint32_t));
            for (uint32_t s = 0; s < header->scripts; ++s) {
                anm_script_t* script = malloc(sizeof(*script));
                script->offset = &(script_offsets[s]);
                list_init(&script->instrs);

                unsigned char* limit = map;
                if (s < header->scripts - 1)
                    limit += script_offsets[s + 1].offset;
                else if (header->thtxoffset)
                    limit += header->thtxoffset;
                else if (header->nextoffset)
                    limit += header->nextoffset;
                else
                    limit += file_size;

                unsigned char* instr_ptr = map + script->offset->offset;
                for (;;) {
                    anm_instr_t* instr;
                    if (header->version == 0) {
                        anm_instr0_t* temp_instr = (anm_instr0_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr0_t) > limit ||
                            (temp_instr->type == 0 && temp_instr->time == 0) ||
                            instr_ptr + sizeof(anm_instr0_t) + temp_instr->length > limit)
                            break;

                        instr = malloc(sizeof(anm_instr_t) + temp_instr->length);
                        instr->type = temp_instr->type;
                        instr->length = sizeof(anm_instr_t) + temp_instr->length;
                        instr->time = temp_instr->time;
                        instr->param_mask = 0;
                        memcpy(instr->data, temp_instr->data, temp_instr->length);

                        list_append_new(&script->instrs, instr);

                        instr_ptr += sizeof(anm_instr0_t) + temp_instr->length;
                    } else {
                        anm_instr_t* temp_instr = (anm_instr_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr_t) > limit ||
                            temp_instr->type == 0xffff ||
                            instr_ptr + temp_instr->length > limit)
                            break;

                        list_append_new(&script->instrs, temp_instr);

                        instr_ptr += temp_instr->length;
                    }
                }

                list_append_new(&entry->scripts, script);
            }
        }

        if (header->hasdata) {
            thtx_header_t* thtx = entry->thtx =
                (thtx_header_t*)(map + header->thtxoffset);
            assert(util_strcmp_ref(thtx->magic, stringref("THTX")) == 0);
            assert(thtx->zero == 0);
            assert(thtx->w * thtx->h * format_Bpp(thtx->format) <= thtx->size);
            assert(
                thtx->format == FORMAT_BGRA8888 ||
                thtx->format == FORMAT_RGB565 ||
                thtx->format == FORMAT_ARGB4444 ||
                thtx->format == (uint16_t)FORMAT_RGBA8888 ||
                thtx->format == FORMAT_GRAY8 ||
                thtx->format == FORMAT_PNG);

            entry->data = thtx->data;
        }

        if (!header->nextoffset)
            break;

        map = map + header->nextoffset;
    }

    return archive;
}

static void
anm_dump(
    FILE* stream,
    const anm_archive_t* anm)
{
    unsigned int entry_num = 0;
    anm_entry_t* entry;

    list_for_each(&anm->entries, entry) {
        const id_format_pair_t* formats = NULL;

        if (entry->header->version == 0) {
            formats = formats_v0;
        } else if (entry->header->version == 2) {
            formats = formats_v2;
        } else if (entry->header->version == 3) {
            formats = formats_v3;
        } else if (entry->header->version == 4 || entry->header->version == 7) {
            formats = formats_v4p;
        } else if (entry->header->version == 8) {
            formats = formats_v8;
        } else {
            fprintf(stderr,
                "%s:%s: could not find a format description for version %u\n",
                argv0, current_input, entry->header->version);
            abort();
        }

        fprintf(stream, "ENTRY #%u, VERSION %u\n", entry_num++, entry->header->version);
        fprintf(stream, "Name: %s\n", entry->name);
        if (entry->name2)
            fprintf(stream, "Name2: %s\n", entry->name2);
        fprintf(stream, "Format: %u\n", entry->header->format);
        fprintf(stream, "Width: %u\n", entry->header->w);
        fprintf(stream, "Height: %u\n", entry->header->h);
        if (entry->header->x != 0)
            fprintf(stream, "X-Offset: %u\n", entry->header->x);
        if (!entry->name2 && entry->header->y != 0)
            fprintf(stream, "Y-Offset: %u\n", entry->header->y);
        if (entry->header->version < 7) {
            fprintf(stream, "ColorKey: %08x\n", entry->header->colorkey);
        }
        if (entry->header->zero3 != 0)
            fprintf(stream, "Zero3: %u\n", entry->header->zero3);
        if (entry->header->version >= 1)
            fprintf(stream, "MemoryPriority: %u\n", entry->header->memorypriority);
        if (entry->header->version >= 8)
            fprintf(stream, "LowResScale: %u\n", entry->header->lowresscale);
        if (entry->header->hasdata) {
            fprintf(stream, "HasData: %u\n", entry->header->hasdata);
            fprintf(stream, "THTX-Size: %u\n", entry->thtx->size);
            fprintf(stream, "THTX-Format: %u\n", entry->thtx->format);
            fprintf(stream, "THTX-Width: %u\n", entry->thtx->w);
            fprintf(stream, "THTX-Height: %u\n", entry->thtx->h);
            fprintf(stream, "THTX-Zero: %u\n", entry->thtx->zero);
        }

        fprintf(stream, "\n");

        sprite_t* sprite;
        list_for_each(&entry->sprites, sprite) {
            fprintf(stream, "Sprite: %u %.f*%.f+%.f+%.f\n",
                sprite->id,
                sprite->w, sprite->h,
                sprite->x, sprite->y);
        }

        fprintf(stream, "\n");

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {

            fprintf(stream, "Script: %d\n", script->offset->id);

            unsigned int instr_num = 0;
            anm_instr_t* instr;
            list_for_each(&script->instrs, instr) {
                const char* format = find_format(formats, instr->type);

                if (!format) {
                    fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, instr->type);
                    abort();
                }

                fprintf(stream, "Instruction #%u: %hd %hu %hu",
                    instr_num++, instr->time, instr->param_mask, instr->type);

                if (instr->length > sizeof(anm_instr_t)) {
                    value_t* values;
                    values = value_list_from_data(value_from_data, (unsigned char*)instr->data, instr->length - sizeof(anm_instr_t), format);
                    if (!values)
                        abort();

                    for (size_t i = 0; values[i].type; ++i) {
                        char* disp;
                        disp = value_to_text(&values[i]);
                        fprintf(stream, " %s", disp);
                        value_free(&values[i]);
                        free(disp);
                    }

                    free(values);
                }

                fprintf(stream, "\n");
            }

            fprintf(stream, "\n");
        }

        fprintf(stream, "\n");
    }
}

#ifdef HAVE_LIBPNG
static void
util_total_entry_size(
    const anm_archive_t* anm,
    const char* name,
    unsigned int* widthptr,
    unsigned int* heightptr)
{
    unsigned int width = 0;
    unsigned int height = 0;

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        if (entry->name == name) {
            if (!entry->header->hasdata)
                return;
            if (entry->header->x + entry->thtx->w > width)
                width = entry->header->x + entry->thtx->w;
            if (entry->header->y + entry->thtx->h > height)
                height = entry->header->y + entry->thtx->h;
        }
    }

    *widthptr = width;
    *heightptr = height;
}

static void
anm_replace(
    const anm_archive_t* anm,
    FILE* anmfp,
    const char* name,
    const char* filename)
{
    const format_t formats[] = {
        FORMAT_RGBA8888,
        FORMAT_BGRA8888,
        FORMAT_RGB565,
        FORMAT_ARGB4444,
        FORMAT_GRAY8,
        FORMAT_PNG
    };
    unsigned int f;
    unsigned int width = 0;
    unsigned int height = 0;
    image_t* image;


    util_total_entry_size(anm, name, &width, &height);
    if (width == 0 || height == 0) {
        /* There's nothing to do. */
        return;
    }

    image = png_read(filename);

    if (width != image->width || height != image->height) {
        fprintf(stderr,
            "%s:%s:%s: wrong image dimensions for %s: %u, %u instead of %u, %u\n",
            argv0, current_input, name, filename, image->width, image->height,
            width, height);
        exit(1);
    }

    for (f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {

        long offset = 0;
        anm_entry_t* entry;
        list_for_each(&anm->entries, entry) {
            if (entry->name == name &&
                entry->thtx->format == formats[f] &&
                entry->header->hasdata) {
                unsigned int y;

                if(formats[f] != FORMAT_PNG) {
                    unsigned char* converted_data = format_from_rgba((uint32_t*)image->data, width * height, formats[f]);

                    if (anmfp) {
                      for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                          if (!file_seek(anmfp,
                                        offset + entry->header->thtxoffset + sizeof(thtx_header_t) + (y - entry->header->y) * entry->thtx->w * format_Bpp(formats[f])))
                              exit(1);
                          if (!file_write(anmfp, converted_data + y * width * format_Bpp(formats[f]) + entry->header->x * format_Bpp(formats[f]), entry->thtx->w * format_Bpp(formats[f])))
                              exit(1);
                      }
                    } else {
                      for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                          memcpy(entry->data + (y - entry->header->y) * entry->thtx->w * format_Bpp(formats[f]),
                                converted_data + y * width * format_Bpp(formats[f]) + entry->header->x * format_Bpp(formats[f]),
                                entry->thtx->w * format_Bpp(formats[f]));
                      }
                    }

                    free(converted_data);
                } else {
                    FILE *f = fopen(filename, "r");
                    if(!f) {
                        fprintf(stderr,
                                "%s:%s:%s: could not open %s\n",
                                argv0, current_input, name, filename);
                        exit(1);
                    }

                    /* XXX: Force size to png size */
                    fseek(f, 0, SEEK_END);
                    entry->thtx->size = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    char *map = malloc(entry->thtx->size);
                    fread(map, entry->thtx->size, 1, f);
                    fclose(f);

                    if (anmfp) {
                        if (!file_seek(anmfp, offset + entry->header->thtxoffset + sizeof(thtx_header_t)))
                            exit(1);
                        if (!file_write(anmfp, map, entry->thtx->size))
                            exit(1);
                    } else {
                        memcpy(entry->data, map, entry->thtx->size);
                    }
                }
            }

            offset += entry->header->nextoffset;
        }
    }

    free(image->data);
    free(image);
}

static void
anm_extract(
    const anm_archive_t* anm,
    const char* name)
{
    const format_t formats[] = {
        FORMAT_GRAY8,
        FORMAT_ARGB4444,
        FORMAT_RGB565,
        FORMAT_BGRA8888,
        FORMAT_RGBA8888,
        FORMAT_PNG
    };
    image_t image;

    unsigned int f, y;

    image.width = 0;
    image.height = 0;
    image.format = FORMAT_RGBA8888;

    util_total_entry_size(anm, name, &image.width, &image.height);

    if (image.width == 0 || image.height == 0) {
        /* Then there's nothing to extract. */
        return;
    }

    image.data = malloc(image.width * image.height * 4);
    /* XXX: Why 0xff? */
    memset(image.data, 0xff, image.width * image.height * 4);

    anm_entry_t* entry;
    for (f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        list_for_each(&anm->entries, entry) {
            if (entry->header->hasdata && entry->name == name && formats[f] == entry->thtx->format) {
                if(formats[f] == FORMAT_PNG) {
                    goto write_raw_png;
                    return;
                } else {
                    unsigned char* temp_data = format_to_rgba(entry->data, entry->thtx->w * entry->thtx->h, entry->thtx->format);
                    for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                        memcpy(image.data + y * image.width * 4 + entry->header->x * 4,
                               temp_data + (y - entry->header->y) * entry->thtx->w * 4,
                               entry->thtx->w * 4);
                    }
                    free(temp_data);
                }
            }
        }
    }

    util_makepath(name);
    png_write(name, &image);

    free(image.data);
    return;

write_raw_png:
    util_makepath(name);
    FILE *out = fopen(name, "wb");
    fwrite(entry->data, entry->thtx->size, 1, out);
    fclose(out);
    free(image.data);
    return;
}

static char*
filename_cut(
    char *line,
    size_t len)
{
    char *p = line;

    assert(line);

    if(len == 0) {
        return line;
    }

    /* isspace(3) is only asking for trouble; we don't parse Unicode anyway. */
#define is_space(c) (c == ' ' || c == '\t')

    while(len > 0 && is_space(*p)) {
        len--;
        p++;
    }

    char *start = p;
    char *end = p;

    while(len > 0 && *p != '\n') {
        if(!is_space(*p)) {
            end = p;
        }
        len--;
        p++;
    }

#undef is_space

    end[len == 0 ? 0 : 1] = '\0';
    return start;
}

static anm_archive_t*
anm_create(
    const char* spec)
{
    FILE* f;
    char line[4096];
    anm_archive_t* anm;
    anm_entry_t* entry = NULL;
    anm_script_t* script = NULL;
    anm_instr_t* instr = NULL;
    unsigned int linenum = 1;

    f = fopen(spec, "r");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, spec, strerror(errno));
        exit(1);
    }

#define ERROR(text, ...) fprintf(stderr, \
    "%s: %s:%d: " text "\n", argv0, spec, linenum, ##__VA_ARGS__)

#define SCAN_DEPRECATED(fieldstr, format, field) \
    if (sscanf(line, fieldstr ": " format, &entry->header->field) > 0) { \
        ERROR("warning: " fieldstr " is an old field written by thanm <= 10; re-dump the spec after ANM creation to remove this warning"); \
    };

    anm = malloc(sizeof(anm_archive_t));
    anm->map = NULL;
    anm->map_size = 0;
    list_init(&anm->names);
    list_init(&anm->entries);

    while (fgets(line, sizeof(line), f)) {
        if (util_strcmp_ref(line, stringref("ENTRY ")) == 0) {
            entry = malloc(sizeof(*entry));
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
            list_append_new(&anm->entries, entry);

            if(sscanf(line, "ENTRY %u", &entry->header->version) > 0) {
                ERROR("warning: No entry number detected. This spec was written by thanm <= 10; re-dump it after ANM creation to remove this warning");
            } else {
                unsigned int temp;
                sscanf(line, "ENTRY #%u, VERSION %u", &temp, &entry->header->version);
            }
        } else if (util_strcmp_ref(line, stringref("Name: ")) == 0) {
            size_t offset = stringref("Name: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            entry->name = anm_get_name(anm, name);
        } else if (util_strcmp_ref(line, stringref("Name2: ")) == 0) {
            size_t offset = stringref("Name2: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            entry->name2 = strdup(name);
        } else if (util_strcmp_ref(line, stringref("Sprite: ")) == 0) {
            sprite_t* sprite = malloc(sizeof(*sprite));
            list_append_new(&entry->sprites, sprite);

            if (5 != sscanf(line, "Sprite: %u %f*%f+%f+%f",
                         &sprite->id, &sprite->w, &sprite->h,
                         &sprite->x, &sprite->y)) {
                ERROR("Sprite parsing failed for %s", line);
                exit(1);
            }
        } else if (util_strcmp_ref(line, stringref("Script: ")) == 0) {
            script = malloc(sizeof(*script));
            script->offset = malloc(sizeof(*script->offset));
            list_init(&script->instrs);
            list_append_new(&entry->scripts, script);
            if (1 != sscanf(line, "Script: %d", &script->offset->id)) {
                ERROR("Script parsing failed for %s", line);
                exit(1);
            }
        } else if (util_strcmp_ref(line, stringref("Instruction")) == 0) {
            char* tmp = line + stringref("Instruction").len;
            char* before;
            char* after = NULL;

            tmp = strchr(tmp, ':');
            if (!tmp) {
                ERROR("Instruction parsing failed for %s", line);
                exit(1);
            }
            tmp++;

            instr = malloc(sizeof(*instr));

            instr->length = 0;
            instr->time = (int16_t)strtol(tmp, &tmp, 10);
            instr->param_mask = strtol(tmp, &tmp, 10);
            instr->type = strtol(tmp, &tmp, 10);

            before = tmp;

            for (;;) {
                int32_t i;
                float f;

                i = strtol(before, &after, 10);
                if (after == before) {
                    break;
                } else {
                    instr->length += sizeof(int32_t);
                    instr = realloc(instr, sizeof(anm_instr_t) + instr->length);
                    if (*after == 'f' || *after == '.') {
                        f = strtof(before, &after);
                        memcpy(instr->data + instr->length - sizeof(float),
                            &f, sizeof(float));
                        /* Skip 'f'. */
                        ++after;
                    } else {
                        memcpy(instr->data + instr->length - sizeof(int32_t),
                            &i, sizeof(int32_t));
                    }
                }

                before = after;
            }

            list_append_new(&script->instrs, instr);
        } else {
            sscanf(line, "Format: %u", &entry->header->format);
            sscanf(line, "Width: %u", &entry->header->w);
            sscanf(line, "Height: %u", &entry->header->h);
            sscanf(line, "X-Offset: %u", &entry->header->x);
            sscanf(line, "Y-Offset: %u", &entry->header->y);

            SCAN_DEPRECATED("Zero2", "%u", colorkey);
            if (sscanf(line, "ColorKey: %08x", &entry->header->colorkey) > 0
                && entry->header->version >= 7)
                ERROR("ColorKey is no longer supported in ANM versions >= 7");

            sscanf(line, "Zero3: %u", &entry->header->zero3);

            SCAN_DEPRECATED("Unknown1", "%u", memorypriority);
            if (sscanf(line, "MemoryPriority: %u", &entry->header->memorypriority)
                && entry->header->version == 0)
                ERROR("MemoryPriority is ignored in ANM version 0");

            SCAN_DEPRECATED("Unknown2", "%hu", lowresscale);
            if(sscanf(line, "LowResScale: %hu", &entry->header->lowresscale)
                && entry->header->version < 8)
                ERROR("LowResScale is ignored in ANM versions < 8");

            sscanf(line, "HasData: %hu", &entry->header->hasdata);
            sscanf(line, "THTX-Size: %u", &entry->thtx->size);
            sscanf(line, "THTX-Format: %hu", &entry->thtx->format);
            sscanf(line, "THTX-Width: %hu", &entry->thtx->w);
            sscanf(line, "THTX-Height: %hu", &entry->thtx->h);
            sscanf(line, "THTX-Zero: %hu", &entry->thtx->zero);
        }
        linenum++;
    }

#undef SCAN_DEPRECATED
#undef ERROR

    fclose(f);

    return anm;
}

static void
anm_write(
    anm_archive_t* anm,
    const char* filename)
{
    FILE* stream;

    stream = fopen(filename, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        sprite_t* sprite;
        anm_script_t* script;
        long base = file_tell(stream);
        unsigned int namepad = 0;
        char* padding;
        unsigned int j;
        unsigned int spriteoffset;

        namepad = (16 - strlen(entry->name) % 16);

        unsigned int sprite_count = 0;
        list_for_each(&entry->sprites, sprite)
            ++sprite_count;

        unsigned int script_count = 0;
        list_for_each(&entry->scripts, script)
            ++script_count;

        file_seek(stream, base +
                          sizeof(anm_header06_t) +
                          sprite_count * sizeof(uint32_t) +
                          script_count * sizeof(anm_offset_t));

        entry->header->nameoffset = file_tell(stream) - base;
        file_write(stream, entry->name, strlen(entry->name));

        padding = calloc(1, namepad);
        file_write(stream, padding, namepad);
        free(padding);

        if (entry->name2 && entry->header->version == 0) {
            namepad = (16 - strlen(entry->name2) % 16);

            entry->header->y = file_tell(stream) - base;
            file_write(stream, entry->name2, strlen(entry->name2));

            padding = calloc(1, namepad);
            file_write(stream, padding, namepad);
            free(padding);
        }

        spriteoffset = file_tell(stream) - base;

        list_for_each(&entry->sprites, sprite)
            file_write(stream, sprite, sizeof(*sprite));

        list_for_each(&entry->scripts, script) {
            script->offset->offset = file_tell(stream) - base;

            anm_instr_t* instr;
            list_for_each(&script->instrs, instr) {
                if (entry->header->version == 0) {
                    anm_instr0_t new_instr;
                    new_instr.time = instr->time;
                    new_instr.type = instr->type;
                    new_instr.length = instr->length;
                    file_write(stream, &new_instr, sizeof(new_instr));
                    if (new_instr.length) {
                        file_write(stream,
                            instr->data,
                            new_instr.length);
                    }
                } else {
                    if (instr->type == 0xffff) {
                        instr->length = 0;
                        file_write(stream, instr, sizeof(*instr));
                    } else {
                        instr->length += sizeof(*instr);
                        file_write(stream, instr, instr->length);
                    }
                }
            }

            if (entry->header->version == 0) {
                anm_instr0_t sentinel = { 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(sentinel));
            } else {
                anm_instr_t sentinel = { 0xffff, 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(sentinel));
            }
        }

        if (entry->header->hasdata) {
            entry->header->thtxoffset = file_tell(stream) - base;

            file_write(stream, entry->thtx, sizeof(thtx_header_t));
            file_write(stream, entry->data, entry->thtx->size);
        }

        if (list_is_last_iteration())
            entry->header->nextoffset = 0;
        else
            entry->header->nextoffset = file_tell(stream) - base;

        entry->header->sprites = sprite_count;
        entry->header->scripts = script_count;

        file_seek(stream, base);

        if (entry->header->version >= 7) {
            convert_header_to_11(entry->header);
            file_write(stream, entry->header, sizeof(anm_header06_t));
            convert_header_to_old(entry->header);
        } else {
            file_write(stream, entry->header, sizeof(anm_header06_t));
        }

        for (j = 0; j < sprite_count; ++j) {
            uint32_t ofs = spriteoffset + j * sizeof(sprite_t);
            file_write(stream, &ofs, sizeof(uint32_t));
        }

        list_for_each(&entry->scripts, script) {
            file_write(stream, script->offset, sizeof(*script->offset));
        }

        file_seek(stream, base + entry->header->nextoffset);
    }

    fclose(stream);
}
#endif

static void
anm_free(
    anm_archive_t* anm)
{
    int is_mapped = anm->map != NULL;

    char* name;
    list_for_each(&anm->names, name)
        free(name);
    list_free_nodes(&anm->names);

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        list_free_nodes(&entry->sprites);

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {
            if (!is_mapped)
                free(script->offset);

            if (!is_mapped || entry->header->version == 0) {
                anm_instr_t* instr;
                list_for_each(&script->instrs, instr) {
                    free(instr);
                }
            }
            list_free_nodes(&script->instrs);

            free(script);
        }
        list_free_nodes(&entry->scripts);

        if (!is_mapped) {
            free(entry->header);
            free(entry->thtx);
            free(entry->name2);
            free(entry->data);

            sprite_t* sprite;
            list_for_each(&entry->sprites, sprite)
                free(sprite);
        } else if (entry->header->version >= 7) {
            free(entry->header);
        }

        free(entry);
    }
    list_free_nodes(&anm->entries);

    if (is_mapped)
        file_munmap(anm->map, anm->map_size);

    free(anm);
}

static void
print_usage(void)
{
#ifdef HAVE_LIBPNG
#define USAGE_LIBPNGFLAGS " | -x | -r "
#else
#define USAGE_LIBPNGFLAGS ""
#endif
    printf("Usage: %s [-c VERSION SPECX] [-Vf] [-l" USAGE_LIBPNGFLAGS "] ARCHIVE ...\n"
           "Options:\n"
           "  -l ARCHIVE            list archive\n", argv0);
#ifdef HAVE_LIBPNG
    printf("  -x ARCHIVE [FILE...]  extract entries\n"
           "  -r ARCHIVE NAME FILE  replace entry in archive\n"
           "  -c VERSION SPECX ARCHIVE  create archive\n"
           "  -m  use map file for translating mnemonics\n"
           "  -I  use include directory for ecl args\n");
#endif
    printf("  -V                    display version information and exit\n"
           "  -f                    ignore errors when possible\n"
           "  -p                    force png (format 11) on all formats\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n");
}

static void
free_maps(void)
{
    eclmap_free(g_eclmap_opcode);
    eclmap_free(g_eclmap_global);
    eclarg_includes_free(g_eclarg_includes);
}

int
main(
    int argc,
    char* argv[])
{
    const char commands[] = ":l"
#ifdef HAVE_LIBPNG
                            "xrc:"
#endif
                            "Vfpm:I:";
    int command = -1;
    int force_png = 0;

    FILE* in;

    anm_archive_t* anm;
#ifdef HAVE_LIBPNG
    anm_entry_t* entry;
    char* name;

    FILE* anmfp;
    int i;
#endif

    g_eclmap_opcode = eclmap_new();
    g_eclmap_global = eclmap_new();
    g_eclarg_includes = eclarg_includes_new();
    atexit(free_maps);

    argv0 = util_shortname(argv[0]);
    int opt;
    int version;
    int ind=0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc,argv,commands)) {
        case 'l':
        case 'x':
        case 'r':
            if(command != -1) {
                fprintf(stderr,"%s: More than one mode specified\n",argv0);
                print_usage();
                exit(1);
            }
            command = opt;
            break;
        case 'c':
            if(command != -1) {
                fprintf(stderr,"%s: More than one mode specified\n",argv0);
                print_usage();
                exit(1);
            }
            command = opt;
            version = parse_version(util_optarg);
            break;
        case 'I':
            eclarg_add_include(g_eclarg_includes, util_optarg);
            break;
        case 'm': {
            FILE* map_file = NULL;
            map_file = fopen(util_optarg, "r");
            if (!map_file) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                        argv0, util_optarg, strerror(errno));
                exit(1);
            }
            eclmap_load(g_eclmap_opcode, g_eclmap_global, map_file, util_optarg);
            fclose(map_file);
            break;
        }
        case 'f':
            option_force = 1;
            break;
        case 'p':
            force_png = 1;
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

    switch (command) {
    case 'l':
        if (argc != 1) {
            print_usage();
            exit(1);
        }

        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);
        anm_dump(stdout, anm);
        anm_free(anm);
        exit(0);
#ifdef HAVE_LIBPNG
    case 'x':
        if (argc < 1) {
            print_usage();
            exit(1);
        }

        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        if (argc == 1) {
            /* Extract all files. */
            list_for_each(&anm->names, name) {
                current_output = name;
                puts(name);
                anm_extract(anm, name);
            }
        } else {
            /* Extract all listed files. */
            for (i = 1; i < argc; ++i) {
                list_for_each(&anm->names, name) {
                    if (strcmp(argv[i], name) == 0) {
                        current_output = name;
                        puts(name);
                        anm_extract(anm, name);
                        goto extract_next;
                    }
                }
                fprintf(stderr, "%s:%s: %s not found in archive\n",
                        argv0, current_input, argv[i]);
            extract_next:
                ;
            }
        }

        anm_free(anm);
        exit(0);
    case 'r':
        if (argc != 3) {
            print_usage();
            exit(1);
        }

        current_output = argv[2];
        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        anmfp = fopen(argv[0], "rb+");
        if (!anmfp) {
            fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                    argv0, current_input, strerror(errno));
            exit(1);
        }

        list_for_each(&anm->names, name) {
            if (strcmp(argv[1], name) == 0) {
                anm_replace(anm, anmfp, name, argv[2]);
                goto replace_done;
            }
        }

        fprintf(stderr, "%s:%s: %s not found in archive\n",
                argv0, current_input, argv[1]);

    replace_done:

        fclose(anmfp);

#if 0
        offset = 0;
        list_for_each(&anm->entries, entry) {
            unsigned int nextoffset = entry->header->nextoffset;
            if (strcmp(argv[1], entry->name) == 0 && entry->header->hasdata) {
                if (!file_seek(anmfp,
                               offset + entry->header->thtxoffset + 4 + sizeof(thtx_header_t)))
                    exit(1);
                if (!file_write(anmfp, entry->data, entry->thtx->size))
                    exit(1);
            }
            offset += nextoffset;
        }
#endif

        anm_free(anm);
        exit(0);
    case 'c':
        if(version != 0 &&
           version != 2 &&
           version != 3 &&
           version != 4 &&
           version != 7 &&
           version != 8) {
            print_usage();
            exit(1);
        }

        FILE *in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, argv[0], strerror(errno));
            exit(1);
        }
        custom_fmts = list_new();
        anm_archive_t *anm = anm_parse(in, version);

        /* Allocate enough space for the THTX data. */
        list_for_each(&anm->entries, entry) {
            if (entry->header->hasdata && entry->name[0] != '@') {
                if(force_png) {
                    entry->thtx->format = FORMAT_PNG;
                }
                /* XXX: There are a few entries with a thtx.size greater than
                 *      w*h*Bpp.  The extra data appears to be all zeroes. */
                entry->data = calloc(1, entry->thtx->size);
            } else {
              entry->data = NULL;
              entry->thtx->size = 0;
            }
        }

        list_for_each(&anm->names, name) {
          if (name[0] != '@') {
            printf("inserting %s\n", name);
            anm_replace(anm, NULL, name, name);
          }
        }

        current_output = argv[1];
        anm_write(anm, argv[1]);

        anm_free(anm);
        list_free_nodes(custom_fmts);
        free(custom_fmts);

#if 0

        current_input = argv[1];
        anm = anm_create(argv[1]);

        /* Allocate enough space for the THTX data. */
        list_for_each(&anm->entries, entry) {
            if (entry->header->hasdata) {
                if(force_png) {
                    entry->thtx->format = FORMAT_PNG;
                }
                /* XXX: There are a few entries with a thtx.size greater than
                 *      w*h*Bpp.  The extra data appears to be all zeroes. */
                entry->data = calloc(1, entry->thtx->size);
            }
        }

        list_for_each(&anm->names, name)
            anm_replace(anm, NULL, name, name);

        current_output = argv[0];
        anm_write(anm, argv[0]);

        anm_free(anm);
#endif
        exit(0);
#endif
    default:
        print_usage();
        exit(1);
    }
}
