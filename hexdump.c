#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
static const char* HEX = "0123456789abcdef";

void hexdump(void *buffer, size_t size) {
    if (size == 0 || buffer == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "!! bad hexdump argument !!\n" ANSI_COLOR_RESET);
        return;
    }
    size_t begin_idx=0, end_idx=0;
    while(end_idx < size) { // print one line
        begin_idx = end_idx;
        end_idx = begin_idx + 16;
        if (end_idx > size) end_idx = size;
        printf(ANSI_COLOR_GREEN "0x%08lx " ANSI_COLOR_RESET, begin_idx);
        for (size_t idx = begin_idx; idx<end_idx; idx++) { // print hex code
            uint8_t chr = ((uint8_t *)buffer)[idx];
            if (idx == begin_idx+8) printf(" ");
            printf(" %c%c", HEX[(chr>>4)&0xf], HEX[chr & 0xf]);
        }
        for (size_t i=end_idx;i<begin_idx+16;i++) {
            if (i == begin_idx+8) printf(" ");
            printf("   ");
        }
        printf(ANSI_COLOR_GREEN "  |" ANSI_COLOR_RESET);

        for (size_t idx = begin_idx; idx<end_idx; idx++) { // print ascii code
            uint8_t chr = ((uint8_t *)buffer)[idx];
            if (isgraph(chr))
                printf("%c", chr);
            else
                printf(ANSI_COLOR_RED "." ANSI_COLOR_RESET);
        }
        for (size_t i=end_idx;i<begin_idx+16;i++)printf(" ");
        printf(ANSI_COLOR_GREEN "|" ANSI_COLOR_RESET);
        printf("\n");
    }
}
