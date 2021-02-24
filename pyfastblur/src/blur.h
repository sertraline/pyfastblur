#define scl source_channel
#define tcl target_channel
#define sigma standard_deviation

// implementation of http://blog.ivank.net/fastest-gaussian-blur.html
int* boxes_for_gauss(int sigma, int num) {
    auto avg_filter_width = sqrt((12 * sigma * sigma) / num + 1);
    int wfloor = floor(avg_filter_width);
    if (wfloor % 2 == 0) wfloor--;
    int wu = wfloor + 2;

    int const_a = (12 * sigma * sigma - num * wfloor * wfloor - 4 * num * wfloor - 3 * num);
    int const_b = (-4 * wfloor - 4);
    int med_ideal = const_a / const_b;
    int med_round = round(med_ideal);

    int* sizes = new int[num];
    for (int i = 0; i < num; i++) {
        sizes[i] = i < med_round ? wfloor : wu;
    }
    return sizes;
}

void box_blur_horizontal(png_byte* scl, png_byte* tcl, int w, int h, int r) {
    float iarr = 1.f / (r + r + 1);
    for (int i = 0; i < h; i++) {
        int ti = i * w;
        int li = ti;
        int ri = ti + r;

        int first_val = scl[ti];
        int last_val = scl[ti + w - 1];
        int val = (r + 1) * first_val;

        for (int j = 0; j < r; j++) val += scl[ti + j];
        for (int j = 0; j <= r; j++) {
            val += scl[ri++] - first_val;
            tcl[ti++] = round(val * iarr);
        }
        for (int j = (r + 1); j < (w - r); j++) {
            val += scl[ri++] - scl[li++];
            tcl[ti++] = round(val * iarr);
        }
        for (int j = (w - r); j < w; j++) {
            val += last_val - scl[li++];
            tcl[ti++] = round(val * iarr);
        }
    }
}

void box_blur_total(png_byte* scl, png_byte* tcl, int w, int h, int r) {
    float iarr = 1.f / (r + r + 1);
    for (int i = 0; i < w; i++) {
        int ti = i;
        int li = ti;
        int ri = ti + r * w;

        int first_val = scl[ti];
        int last_val = scl[ti + w * (h - 1)];
        int val = (r + 1) * first_val;

        for (int j = 0; j < r; j++) val += scl[ti + j * w];
        for (int j = 0; j <= r; j++) {
            val += scl[ri] - first_val;
            tcl[ti] = round(val * iarr);
        }
        for (int j = (r + 1); j < (h - r); j++) {
            val += scl[ri] - scl[li];
            tcl[ti] = round(val * iarr);
        }
        for (int j = (h - r); j < h; j++) {
            val += last_val - scl[li];
            tcl[ti] = round(val * iarr);
        }
    }
}

void box_blur_full(png_byte* scl, png_byte* tcl, int w, int h, int r) {
    for (int i = 0; i < (w * h); i++) {
        tcl[i] = scl[i];
    }
    box_blur_horizontal(tcl, scl, w, h, r);
    box_blur_total(scl, tcl, w, h, r);
}

void gaussian_blur(png_byte* scl, png_byte* tcl, int w, int h, int r, int sb) {
    auto boxes = boxes_for_gauss(r, 3);
    box_blur_full(scl, tcl, w, h, (boxes[0] - 1) / 2);
    if (sb) {
        box_blur_full(tcl, scl, w, h, (boxes[1] - 1) / 2);
        box_blur_full(scl, tcl, w, h, (boxes[2] - 1) / 2);
    }
    delete[] boxes;
}