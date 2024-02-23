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

void blend_alpha(BMP_color *pixel, BMP_color color, double alpha) {
    alpha *= -1;
    pixel->red += ((int)pixel->red - (int)color.red) * alpha;
    pixel->green += ((int)pixel->green - (int)color.green) * alpha;
    pixel->blue += ((int)pixel->blue - (int)color.blue) * alpha;
}

void render_dot(int size, BMP_color color, double alpha, BMP_color *framebuffer, int w, int h, int x, int y) {
    if (size == 1) {
        blend_alpha(&framebuffer[y * w + x], color, alpha);
    }

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
            blend_alpha(&framebuffer[ya * w + xa], color, alpha);
        }
    }
}

void plot_function(Expression function, BMP_color color, BMP_color *framebuffer, int w, int h, Value scale, int step, int size) {
    const int halfw = w / 2;
    const int halfh = h / 2;
    
    State state;
    State_init(&state);
    state.vars['x'].occupied = 1;

    Value *xp = &state.vars['x'].value;

    *xp = 0;

    Result r = Expression_evaluate(function, &state);
    if (r.error) {
        fprintf(stderr, "Evaluation error: %s\n", r.error);
        free(r.error);
        return;
    }

    for (int x = 0; x < w; x+=step) {
        *xp = (Value)(x - halfw) / scale;
        Result r = Expression_evaluate(function, &state);
        int y = (int)((r.value) * scale) + halfh;
        //fprintf(stderr, "x: %i y: %i xv: %lf yv: %lf\n", x, y, *xp, r.value);
        if (y < 0 || y >= h) {
            continue;
        }
        render_dot(size, color, 1, framebuffer, w, h, x, y);
    }
}

const Value treshold_multiplier = 0.1;
const int max_depth = 8;

double refine_equation(Expression equation, State *state, Value *xp, Value *yp, Value treshold, Value pixel_size, int depth) {
    Result r = Expression_evaluate(equation, state);

    if (Value_fabs(r.value) > treshold) {
        return 0;
    }
    
    if (depth == max_depth) {
        return 1;
    }

    Value ox = *xp;
    Value oy = *yp;

    double alpha = 0.1;
    const double alpha_per = (1.0 - alpha) * 0.25 * (1.0 + (double)depth);

    *xp -= pixel_size * 0.25;
    *yp -= pixel_size * 0.25;
    alpha += alpha_per * refine_equation(equation, state, xp, yp, treshold * treshold_multiplier, pixel_size * 0.5, depth + 1);
    *xp += pixel_size * 0.25;
    alpha += alpha_per * refine_equation(equation, state, xp, yp, treshold * treshold_multiplier, pixel_size * 0.5, depth + 1);
    *yp += pixel_size * 0.25;
    alpha += alpha_per * refine_equation(equation, state, xp, yp, treshold * treshold_multiplier, pixel_size * 0.5, depth + 1);
    *xp -= pixel_size * 0.25;
    alpha += alpha_per * refine_equation(equation, state, xp, yp, treshold * treshold_multiplier, pixel_size * 0.5, depth + 1);

    *xp = ox;
    *yp = oy;

    if (alpha > 1.0) {
        alpha = 1.0;
    }

    return alpha;
}

void plot_equation(Expression equation, Value treshold, BMP_color color, BMP_color *framebuffer, int w, int h, Value scale, int step, int size) {
    const int halfw = w / 2;
    const int halfh = h / 2;
    
    State state;
    State_init(&state);
    state.vars['x'].occupied = 1;
    state.vars['y'].occupied = 1;

    Value *xp = &state.vars['x'].value;
    Value *yp = &state.vars['y'].value;

    *xp = 0;
    *yp = 0;

    Result r = Expression_evaluate(equation, &state);
    if (r.error) {
        fprintf(stderr, "Evaluation error: %s\n", r.error);
        free(r.error);
        return;
    }

    const Value scale_inv = 1 / scale;

    for (int x = 0; x < w; x += step) {
        *xp = ((Value)(x - halfw) + 0.5) * scale_inv;
        for (int y = 0; y < h; y += step) {
            *yp = ((Value)(y - halfh) + 0.5) * scale_inv;
            double alpha = refine_equation(equation, &state, xp, yp, treshold, scale_inv, 0);
            if (alpha == 0) {
                continue;
            }

            render_dot(size, color, alpha, framebuffer, w, h, x, y);
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
const Value scale = 256;
const Value treshold = 0.01;

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


    const int w = 1024;
    const int h = 1024;
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