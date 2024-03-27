#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bmp_header.h"

struct color {
    char B;  // blue
    char G;  // green
    char R;  // red
};

struct position {
    int x;
    int y;
};

struct queue {
    struct position pos;
    struct queue* next;
};

struct bmp {
    bmp_fileheader file_header;
    bmp_infoheader info_header;
    int padding_size;
    struct color ** matrix;
};

void mem_cleanup(struct bmp * bitmap) {
    if (bitmap == NULL || bitmap->matrix == NULL) {
        return;
    }
    for (int i = 0; i < bitmap->info_header.height; i++) {
        if (bitmap->matrix[i] == NULL)
            continue;
        free(bitmap->matrix[i]);
    }
    free(bitmap->matrix);
}

void read_from_path(struct bmp ** out_bitmap, char * path) {
    if (*out_bitmap != NULL) {
         mem_cleanup(*out_bitmap);
         free(*out_bitmap);
    }
    *out_bitmap = malloc (sizeof(struct bmp));
    struct bmp * bitmap = *out_bitmap;
    printf("Reading from path %s \n", path);
    FILE * input = NULL;
    input = fopen(path, "r");
    // read the structure
    fread(&bitmap->file_header, sizeof(bmp_fileheader), 1, input);
    fread(&bitmap->info_header, sizeof(bmp_infoheader), 1, input);
    printf("file size %d\n", bitmap->file_header.bfSize);
    printf("width %d\n", bitmap->info_header.width);
    printf("width %d\n", bitmap->info_header.height);
    printf("image size %d\n", bitmap->info_header.biSizeImage);
    int wh = bitmap->info_header.width * bitmap->info_header.height;
    bitmap->padding_size = (int)(4-sizeof(struct color) * wh %4) %4;
    // read the image buffer + padding
    bitmap->matrix = malloc(sizeof (struct color *) * bitmap->info_header.height);
    for (int i = 0 ; i < bitmap->info_header.height; i++) {
        bitmap->matrix[i] = malloc (sizeof(struct color) * bitmap->info_header.width);
        fread(bitmap->matrix[i], bitmap->info_header.width * sizeof(struct color), 1, input);
        fseek(input, bitmap->padding_size, SEEK_CUR);
    }
    fclose(input);
}

void write_to_path(struct bmp* bitmap, char * path) {
    // open file in write mode
    FILE * output = NULL;
    output = fopen(path, "w");
    // write the structure
    fwrite(&bitmap->file_header, sizeof(bmp_fileheader), 1, output);
    fwrite(&bitmap->info_header, sizeof(bmp_infoheader), 1, output);
    // write the image buffer + padding
    for (int i = 0 ; i < bitmap->info_header.height; i++) {
        fwrite(bitmap->matrix[i], bitmap->info_header.width * sizeof(struct color), 1, output);
        char pad = 0;
        for (int j = 0; j < bitmap->padding_size; j++) {
            fwrite(&pad, sizeof(char), 1, output);
        }
    }
    printf("file write successful: %ld bytes\n", ftell(output));

    fclose(output);
}

void draw_pixel(struct bmp* bitmap, int y, int x, int width, struct color draw_color) {
    for (int i = y - width/2; i < width + y - width/2; i++) {
       for (int j = x - width/2; j < width + x - width/2; j++) {
           // check for bounds
           if (i < 0 || i > bitmap->info_header.height - 1 || j < 0 || j > bitmap->info_header.width) {
                continue;
           }
           // put pixel
           bitmap->matrix[i][j] = draw_color;
       }
    }
}

int min(int a, int b) {
    return a > b ? b : a;
}

int max(int a, int b) {
    return a < b ? b : a;
}

void draw_line(struct bmp* bitmap, int y1, int x1, int y2, int x2, int width, struct color draw_color) {
    // draw the (x1, y1) and (x2, y2) pixels
    draw_pixel(bitmap, x1, y1, width, draw_color);
    draw_pixel(bitmap, x2, y2, width, draw_color);
    int ymin = min(y2, y1);
    int ymax = max(y2, y1);
    int xmin = min(x2, x1);
    int xmax = max(x2, x1);
    int slope = (x2-x1) * (y2-y1);
    if (ymax - ymin > xmax - xmin) {
        for (int j = ymin+1; j < ymax; j++) {
            // calculate x, draw pixel
            int i = 0;
            int factor = ((j - ymin)*(x2 - x1))/(y2 - y1);
            if (slope >= 0) {
                i = xmin + factor;
            } else {
                if ((j - ymin)*(x2 - x1) % (y2 - y1) != 0 
                && abs(factor * (y2 - y1)) < abs((j - ymin)*(x2 - x1))){
                    // force rounding down negative numbers
                    factor -= 1;
                }
                i = xmax + factor;
            }

            draw_pixel(bitmap, i, j, width, draw_color);
        }
    } else {
        for (int i = xmin+1; i < xmax; i++) {
            // calculate y, draw pixel
            int j = 0;
            int factor = ((i - xmin)*(y2 - y1))/(x2 - x1);
            if (slope >= 0) {
                j = ymin + factor;
            } else {
                if ((i - xmin)*(y2 - y1) % (x2 - x1) != 0 
                && abs(factor * (x2 - x1)) < abs((i - xmin)*(y2 - y1))){
                    // force rounding down negative numbers
                    factor -= 1;
                }
                j = ymax + factor;
            }
            draw_pixel(bitmap, i, j, width, draw_color);
        }
    }
}

int console() {
    printf("Console ready. You may type your command.\n");
    struct color draw_color = {0, 0, 0};
    int line_width = 0;
    struct bmp * main_image = NULL;
    struct bmp * insert_image = NULL;
    char * insert_path = "";
    int quit = 0;
    int const cmd_size = 1024;
    int real_size = 0;
    char* command = (char*)malloc(cmd_size * sizeof(char));
    while (!quit) {
        printf(">");
        // clear command string
        memset(command, 0, cmd_size);
        // get command line as string
        fgets(command, cmd_size, stdin);
        real_size = (int) (strlen(command) - 1);
        if (command[real_size] == '\n')
            command[real_size] = 0;
        char * token = strtok(command, " ");
        if (strncmp(token, "save", real_size) == 0) {
            // the path
            char * path = strtok(NULL, " ");
            write_to_path(main_image, path);
            continue;
        } else if (strncmp(token, "edit", real_size) == 0) {
            // the path
            char * path  = strtok(NULL, " ");
            read_from_path(&main_image, path);
            continue;
        } else if (strncmp(token, "insert", real_size) == 0) {
            // the path
            char * path  = strtok(NULL, " ");
            // y
            int y = atoi(strtok(NULL, " "));
            // x
            int x = atoi(strtok(NULL, " "));
            read_from_path(&insert_image, path);
            int line_size = main_image->info_header.width - y;
            if (line_size > insert_image->info_header.width)
                line_size = insert_image->info_header.width;
            for (int i = 0; i < insert_image->info_header.height; i++) {
                if (i + x >= main_image->info_header.height)
                    break;
                memcpy(main_image->matrix[i + x] + y, insert_image->matrix[i], line_size * sizeof(struct color));
            }
            mem_cleanup(insert_image);
            free(insert_image);
            insert_image = NULL;
            continue;
        } else if (strncmp(token, "set", real_size) == 0) {
            // set what
            token  = strtok(NULL, " ");
            if (strncmp(token, "draw_color", strlen(token)) == 0) {
                // R G B
                int r = atoi(strtok(NULL, " "));
                int g = atoi(strtok(NULL, " "));
                int b = atoi(strtok(NULL, " "));
                draw_color.B = (char)b;
                draw_color.G = (char)g;
                draw_color.R = (char)r;
                continue;
            } else if (strncmp(token, "line_width", strlen(token)) == 0) {
                // x
                int x = atoi(strtok(NULL, " "));
                line_width = x;
                continue;
            }
        } else if (strncmp(token, "draw", real_size) == 0) {
            // all operations are complications of the draw_line function
            token  = strtok(NULL, " ");
            if (strncmp(token, "line", strlen(token)) == 0) {
                // y1 x1 y2 x2
                int y1 = atoi(strtok(NULL, " "));
                int x1 = atoi(strtok(NULL, " "));
                int y2 = atoi(strtok(NULL, " "));
                int x2 = atoi(strtok(NULL, " "));
                draw_line(main_image, y1, x1, y2, x2, line_width, draw_color);
                continue;
            } else if (strncmp(token, "rectangle", strlen(token)) == 0) {
                // y1 x1 width height
                int y1 = atoi(strtok(NULL, " "));
                int x1 = atoi(strtok(NULL, " "));
                int width = atoi(strtok(NULL, " "));
                int height = atoi(strtok(NULL, " "));
                // use the draw_line function to draw 4 lines
                draw_line(main_image, y1, x1, y1 + width, x1, line_width, draw_color);
                draw_line(main_image, y1, x1, y1, x1 + height, line_width, draw_color);
                draw_line(main_image, y1 + width, x1, y1 + width, x1 + height, line_width, draw_color);
                draw_line(main_image, y1, x1 + height, y1 + width, x1 + height, line_width, draw_color);
                continue;
            } else if (strncmp(token, "triangle", strlen(token)) == 0) {
                // y1 x1 y2 x2 y3 x3
                int y1 = atoi(strtok(NULL, " "));
                int x1 = atoi(strtok(NULL, " "));
                int y2 = atoi(strtok(NULL, " "));
                int x2 = atoi(strtok(NULL, " "));
                int y3 = atoi(strtok(NULL, " "));
                int x3 = atoi(strtok(NULL, " "));
                // use the draw_line function to draw 3 lines
                draw_line(main_image, y1, x1, y2, x2, line_width, draw_color);
                draw_line(main_image, y1, x1, y3, x3, line_width, draw_color);
                draw_line(main_image, y3, x3, y2, x2, line_width, draw_color);
                continue;
            }
        } else if (strncmp(token, "fill", real_size) == 0) {
            // y
            int y = atoi(strtok(NULL, " "));
            // x
            int x = atoi(strtok(NULL, " "));
            continue;
        } else if (strncmp(token, "quit", real_size) == 0) {
            quit = 1;
            mem_cleanup(main_image);
            free(main_image);
            // free(image_buffer);
            continue;
        } else {
            // for debug 
            printf("Unrecognized command: ");
            while (token != NULL) {
                printf("%s ", token);
                token = strtok(NULL, " ");
            }
            printf("\n");
        }
    }
    // clean up after myself
    free(command);
    return 0;
}
int main() {
    printf("Test\n");
    return console();
}
