#define main mathengine_main
#ifdef MATH_SW
// Math library using hard coded switch statements for dispatching method calls
#include "mathengine_sw.c"
#else
// Math library using interfaces for dispatching method calls
#include "mathengine_if.c"
#endif
#undef main

#include "bmp.c"

#include <malloc.h>
#include <time.h>

void render_dot(int size, BMP_color color, BMP_color *framebuffer, int w, int h, int x, int y) {
    int xo = x - size / 2;
    int yo = y - size / 2;
    int xt = xo + size;
    int yt = yo + size; 
    if (xo < 0) { xo = 0; }
    if (yo < 0) { yo = 0; }
    if (xt >= w) { xt = w; }
    if (yt >= w) { yt = h; }

    for (int ya = yo; ya < yt; ya++) {
        for (int xa = xo; xa < xt; xa++) {
            framebuffer[ya * w + xa] = color;
        }
    }
}

void plot_function(Expression function, BMP_color color, BMP_color *framebuffer, int w, int h, double scale, int step, int size) {
    const int halfw = w / 2;
    const int halfh = h / 2;
    
    State state;
    State_init(&state);
    state.vars['x'].occupied = 1;

    Value *xp = &state.vars['x'].value;

    for (int x = 0; x < w; x+=step) {
        *xp = (double)(x - halfw) / scale;
        Result r = Expression_evaluate(function, &state);
        if (r.error) {
            fprintf(stderr, "Evaluation error: %s\n", r.error);
            return;
        }
        int y = (int)((r.value) * scale) - halfh + h;
        //fprintf(stderr, "x: %i y: %i xv: %lf yv: %lf\n", x, y, *xp, r.value);
        if (y < 0 || y >= h) {
            continue;
        }
        if (size == 1) {
            framebuffer[y * w + x] = color;
        } else {
            render_dot(size, color, framebuffer, w, h, x, y);
        }
    }
}

void plot_equation(Expression equation, double treshold, BMP_color color, BMP_color *framebuffer, int w, int h, double scale, int step, int size) {
    const int halfw = w / 2;
    const int halfh = h / 2;
    
    State state;
    State_init(&state);
    state.vars['x'].occupied = 1;
    state.vars['y'].occupied = 1;

    Value *xp = &state.vars['x'].value;
    Value *yp = &state.vars['y'].value;

    for (int x = 0; x < w; x += step) {
        *xp = (double)(x - halfw) / scale;
        for (int y = 0; y < h; y += step) {
            *yp = (double)(y - halfh) / scale;
            Result r = Expression_evaluate(equation, &state);
            if (r.error) {
                fprintf(stderr, "Evaluation error: %s\n", r.error);
                return;
            }

            if (fabs(r.value) > treshold) {
                continue;
            }

            if (size == 1) {
                framebuffer[y * w + x] = color;
            } else {
                render_dot(size, color, framebuffer, w, h, x, y);
            }
        }
    }
}

void benchmark_expression(Expression expression, int w, int h) {
    State state;
    State_init(&state);
    state.vars['x'].occupied = 1;
    state.vars['y'].occupied = 1;

    Value *xp = &state.vars['x'].value;
    Value *yp = &state.vars['y'].value;

    clock_t c_begin = clock();

    for (int x = 0; x < w; x++) {
        *xp = (double)x;
        for (int y = 0; y < h; y++) {
            *yp = (double)y;
            Result r = Expression_evaluate(expression, &state);
            if (r.error) {
                fprintf(stderr, "Evaluation error: %s\n", r.error);
                return;
            }
        }
    }

    clock_t c_end = clock();

    fprintf(stderr, "Clocks taken for %d executions: %ld\n", w * h, c_end - c_begin);
}

const BMP_color colors[] = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
};

const int step = 1;
const int size = 1;
const double scale = 256;
const double treshold = 0.005;

enum PlotType {
    FUNCTION, EQUATION, BENCHMARK
};

void plot_expression(enum PlotType type, const char *source, BMP_color *fb, int w, int h) {
    static int color_index = 0;
    char in[512];
    strcpy(in, source);
    char *cursor = in;
    ParserResult result = parseExpression(&cursor);
    if (result.error) {
        fprintf(stderr, "Parser error: %s\n", result.error);
        free(result.error);
        return;
    }
    fprintf(stderr, "Expression: ");
    Expression_print(result.expression, stderr);
    fprintf(stderr, "\n");

    switch (type) {
        case FUNCTION:
            plot_function(result.expression, colors[color_index], fb, w, h, scale, step, size);
            break;
        case EQUATION:
            plot_equation(result.expression, treshold, colors[color_index], fb, w, h, scale, step, size);
            break;
        case BENCHMARK:
            benchmark_expression(result.expression, w, h);
    }

    color_index = (color_index + 1) % ARRLEN(colors);

    Expression_destroy(result.expression);
    Expression_free(result.expression);
}

int main(int argc, const char **argv) {
    int code = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s (output file) (math expression)\n", argv[0]);
        return 1;
    }
    
    FILE *out = fopen(argv[1], "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open file for writing\n");
        code = 1;
        goto cleanup;
    }


    const int w = 2048;
    const int h = 2048;
    BMP_color *framebuffer = malloc(sizeof(BMP_color) * w * h);
    memset(framebuffer, 255, sizeof(BMP_color) * w * h);

    BMP_color clr_black = { 0, 0, 0 };

    for (int x = 0; x < w; x++) {
        framebuffer[(h / 2) * w + x] = clr_black;
    }
    
    for (int y = 0; y < h; y++) {
        framebuffer[y * w + (w / 2)] = clr_black;
    }

    for (int i = 2; i < argc; i++) {
        const char *source = argv[i];
        enum PlotType type = FUNCTION;
        if (source[1] == '=') {
            switch (source[0]) {
                case 'E':
                    type = EQUATION;
                    break;
                case 'B':
                    type = BENCHMARK;
                    break;
                default:
            }
            source += 2;
        }
        plot_expression(type, source, framebuffer, w, h);
    }

    BMP_create(out, w, h, framebuffer);

    cleanup:;
    if (out != NULL) {
        fclose(out);
    }
    return code;

}