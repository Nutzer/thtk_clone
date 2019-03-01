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
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "eclarg.h"

void
eclarg_includes_free(
  eclarg_includes_t* includes
  ) {
  char *ent;
  if(!includes) return;

  list_for_each(includes, ent) {
      free(ent);
  }

  list_free_nodes(includes);
  free(includes);
}

void
eclarg_add_include(
  eclarg_includes_t *includes,
  char *path
  ) {
    list_append_new(includes, strdup(path));
}

FILE *
eclarg_get_file(
  eclarg_includes_t *includes,
  char *fname,
  char *opt
  ) {
    FILE *f;
    f = fopen(fname, opt);
    if(!f) {
        char *ent;
        list_for_each(includes, ent) {
          char path[256];
          strncpy(path, ent, 255);
          strncat(path, "/", 255 - strlen(ent));
          strncat(path, fname, 255 - strlen(ent) - 1);
          f = fopen(path, opt);
          if(f) break;
        }
    }
    return f;
}

char *
eclarg_get_str(
  char *str,
  int pos,
  int str_size
  ) {
    while(str[++pos] != '"') {
        if(pos >= str_size) {
            fprintf(stderr, "argument out-of-file-range");
            exit(1);
        }
    }
    char result[256];
    memset(result, 0, 256);
    char *rpos = result;
    while(str[++pos] != '"') {
        if(pos >= str_size || (uintptr_t)result >= (uintptr_t)result+256) {
            fprintf(stderr, "argument out-of-file-range");
            exit(1);
        }
        *rpos++ = str[pos];
    }

    return strdup(result);
}

