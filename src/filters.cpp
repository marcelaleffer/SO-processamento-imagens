#include "filters.h"

void apply_negative(PGM* in, PGM* out, int row_start, int row_end) {
    for (int i = row_start; i < row_end; i++) {
        for (int j = 0; j < in->w; j++) {
            out->data[i * in->w + j] = 255 - in->data[i * in->w + j];
        }
    }
}

void apply_slice(PGM* in, PGM* out, int row_start, int row_end, int t1, int t2) {
    for (int i = row_start; i < row_end; i++) {
        for (int j = 0; j < in->w; j++) {
            int pixel = in->data[i * in->w + j];
            if (pixel <= t1 || pixel >= t2)
                out->data[i * in->w + j] = 255;
            else
                out->data[i * in->w + j] = pixel;
        }
    }
}
