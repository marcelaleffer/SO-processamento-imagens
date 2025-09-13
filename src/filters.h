#ifndef FILTERS_H
#define FILTERS_H

#include "pgm_utils.h"

// Funções de filtro
void apply_negative(PGM* in, PGM* out, int row_start, int row_end);
void apply_slice(PGM* in, PGM* out, int row_start, int row_end, int t1, int t2);

#endif
