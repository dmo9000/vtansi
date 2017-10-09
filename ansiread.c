#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

extern int errno;


#define CONSOLE_WIDTH		80
#define CONSOLE_HEIGHT	25

#define CHUNK_SIZE	128
#define BUF_SIZE 		4096
#define MAX_ANSI		64				/* maximum length allowed for an ANSI sequence */
#define MAX_PARAMS	16

static char filebuf[4096];
static char ansibuf[MAX_ANSI];
int parameters[MAX_PARAMS];

#define SEQ_ERR							0
#define SEQ_NONE						1
#define SEQ_ESC_1B					2
#define SEQ_ANSI_5B					3
#define SEQ_ANSI_IPARAM			4
#define SEQ_ANSI_CMD_M			5
#define SEQ_ANSI_CMD_J			6
#define SEQ_ANSI_CMD_B			7
#define SEQ_ANSI_EXECUTED		8

static char *states[] = {
    "SEQ_ERR",
    "SEQ_NONE",
    "SEQ_ESC_1B",
    "SEQ_ANSI_5B",
    "SEQ_ANSI_IPARAM",
    "SEQ_ANSI_CMD_M",
    "SEQ_ANSI_CMD_J",
    "SEQ_ANSI_CMD_B",
    "SEQ_ANSI_EXECUTED"
};


int cursor_x = 0;
int cursor_y = 0;

char ansi_mode = 0;
char last_ansi_mode = 0;
int ansioffset = 0;
int paramidx = 0;

int decode_1B(char);
int decode_5B(char);
int decode_command(char);
int decode_integer_parameter(char);

int ansi_decode_cmd_m();									/* text attributes, foreground and background color handling */
int ansi_decode_cmd_J();									/* "home" command (2J) */
int ansi_decode_cmd_B();									/* move down n lines */

void init_parameters();
const char *ansi_state(int s);

int main(int argc, char *argv[])
{
    FILE *ansfile = NULL;
    char *bufptr = NULL;
    char *scanptr = NULL;
    char *endptr = NULL;
    off_t offset = 0;
    size_t elements_read = 0;
    size_t bytes_read = 0;
    char c = 0;
    char last_c = 0;
    int i = 0;

    if (argc < 2) {
        printf("usage: ansiread <filename.ans>\n");
        exit(1);
    }

    ansfile = fopen(argv[1], "rb");

    if (!ansfile) {
        printf("cannot open: %s: %s\n", argv[1], strerror(errno));
        exit(1);
    }

    bufptr = (char *) &filebuf;
    elements_read = fread((char *) bufptr, CHUNK_SIZE, 1, ansfile);
    bytes_read = (elements_read * CHUNK_SIZE);
    printf("[%d] bytes read to buffer\n", bytes_read);
    endptr = (char *) bufptr + (bytes_read);

    last_ansi_mode = SEQ_NONE;
    ansi_mode = SEQ_NONE;

    while (offset < bytes_read) {
        /* get next character from stream */
        scanptr = bufptr + offset;
        last_c = c;
        c = scanptr[0];
        switch(ansi_mode) {
        case SEQ_ERR:
            printf("--\n0x%04x: error in state, ansi_mode = [%s], last_ansi_mode = [%s], character = 0x%02x '%c'\n", offset,
                   (const char *) ansi_state(ansi_mode), (const char *) ansi_state(last_ansi_mode), last_c, (last_c < 32 ? '.' : last_c));
            printf("ANSIBUF[0x%02x]: ", ansioffset);
            for (i = 0; i < ansioffset; i++) {
                if (ansibuf[i] < 32) {
                    printf("\\x%02x", ansibuf[i]);
                } else {
                    printf("%c", ansibuf[i]);
                }
            }
            printf("\n");
            exit(1);
            break;
        case SEQ_NONE:
            last_ansi_mode = ansi_mode;
            ansi_mode = decode_1B(c);
            offset++;
            break;
        case SEQ_ESC_1B:
            last_ansi_mode = ansi_mode;
            ansi_mode = decode_5B(c);
            offset++;
            break;
        case SEQ_ANSI_5B:
            last_ansi_mode = ansi_mode;
            ansi_mode = decode_command(c);
            offset++;
            break;
        case SEQ_ANSI_IPARAM:
            last_ansi_mode = ansi_mode;
            ansi_mode = decode_integer_parameter(c);
            offset++;
            break;
        case SEQ_ANSI_EXECUTED:
            last_ansi_mode = ansi_mode;
            ansi_mode = SEQ_NONE;
            printf("EXECUTED: < ");
            for (i = 0; i < ansioffset; i++) {
                if (ansibuf[i] < 32) {
                    printf("\\x%02x", ansibuf[i]);
                } else {
                    printf("%c", ansibuf[i]);
                }
            }
            printf(" >\n");
            break;
        default:
            printf("--\n0x%04x: error in state [UNHANDLED], ansi_mode = [%s], last_ansi_mode = [%s], character = 0x%02x\n", offset,
                   (const char*) ansi_state(ansi_mode), (const char *) ansi_state(last_ansi_mode), last_c);
            printf("ANSIBUF[0x%02x]: ", ansioffset);
            for (i = 0; i < ansioffset; i++) {
                if (ansibuf[i] < 32) {
                    printf("\\x%02x", ansibuf[i]);
                } else {
                    printf("%c", ansibuf[i]);
                }
            }
            printf("\n");
            exit(0);
            break;
        }
    }
    printf("\n");
    printf("[reached end of buffer]\n");
    fclose(ansfile);
    exit (0);
}

int decode_1B(char c)
{
    int i = 0;

    // printf("decode_1B(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
    if (ansi_mode == SEQ_NONE && c == 0x1b) {
        ansioffset = 0;
        memset(&ansibuf, 0, MAX_ANSI);
        ansibuf[ansioffset] = c;
        ansioffset++;
        init_parameters();
        return SEQ_ESC_1B;
    }

    return SEQ_NONE;
}

int decode_5B(char c)
{
     printf("decode_5B(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
    if (ansi_mode == SEQ_ESC_1B && c == '[') {
        ansibuf[ansioffset] = c;
        ansioffset++;
        return SEQ_ANSI_5B;
    }


    return SEQ_ERR;
}

int decode_command(char c)
{
    printf("decode_command(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
    if (ansi_mode == SEQ_ANSI_5B) {
        if (isdigit(c)) {
            if (parameters[paramidx] == -1) {
                parameters[paramidx] = (c - 48);
            } else {
                printf("decode_command: starting new integer parameter, but parameter[%d] was not -1\n", paramidx);
                return SEQ_ERR;
            }
            printf("decode_command: first digit = 0x%02x '%c'\n", c, c);
            ansibuf[ansioffset] = c;
            ansioffset++;
            return SEQ_ANSI_IPARAM;
        }
        return SEQ_ERR;
    }
    return SEQ_ERR;
}


int decode_integer_parameter(char c)
{
    if (ansi_mode != SEQ_ANSI_IPARAM) {
        return SEQ_ERR;
    }

    ansibuf[ansioffset] = c;
    ansioffset++;

    if (!isdigit(c)) {
        switch (c) {
        case 'B':
            /* end of previous command parameter, increment and dispatch 'B' command */
            paramidx++;
            ansi_mode = SEQ_ANSI_CMD_B;
            return ansi_decode_cmd_B();
            break;
        case 'J':
            /* end of previous command parameter, increment and dispatch 'J' command */
            paramidx++;
            ansi_mode = SEQ_ANSI_CMD_J;
            return ansi_decode_cmd_J();
        case 'm':
            /* end of previous command parameter, increment and dispatch 'm' command */
            paramidx++;
            ansi_mode = SEQ_ANSI_CMD_M;
            return ansi_decode_cmd_m();
        default:
            printf("decode_integer_parameter:   non-digit = 0x%02x '%c'\n", c, c);
            return SEQ_ERR;
            break;
        };
    }

    //printf("decode_integer_parameter:  next digit = 0x%02x '%c'\n", c, c);
    parameters[paramidx] *= 10;
    parameters[paramidx] += (c - 48);
    return SEQ_ANSI_IPARAM;

}

const char *ansi_state(int s)
{

    return (const char *) states[s];

}


void init_parameters()
{
    int i = 0;
    for (i = 0 ; i < MAX_PARAMS; i++) {
        parameters[i] = -1;
    }
    paramidx = 0;
}


int ansi_decode_cmd_m()
{
    int i = 0;
    printf("ansi_decode_cmd_m(%d parameters)\n", paramidx);
    for (i = 0 ; i < paramidx; i++) {
        printf("parameter %d -> %d\n", i, parameters[i]);
        switch(parameters[i]) {
        case 40:
            printf("> set background color = %d\n", parameters[i] - 40);
            break;
        default:
            printf("ansi_decode_cmd_m() - unknown parameter value = %d", parameters[i]);
            return SEQ_ERR;
            break;
        }

    }
    /* processed entirety of 'm' command */
    return SEQ_ANSI_EXECUTED;
}

int ansi_decode_cmd_J()
{
    printf("ansi_decode_cmd_m(%d parameters)\n", paramidx);
    if (paramidx != 1) {
        printf("ansi_decode_cmd_J() - invalid parameter count = %d", paramidx);
        return SEQ_ERR;
    }
    /* check the parameter0 is a '2', ie. for '2J' */
    switch (parameters[0]) {
    case 2:
        printf("> ANSI escape 2J\n");
        cursor_x = 0;
        cursor_y = 0;
        /* processed entirety of 'J' command */
        return SEQ_ANSI_EXECUTED;
        break;
    }
    printf("> ANSI escape [%d]J???\n", parameters[0]);
    return SEQ_ERR;

}

int ansi_decode_cmd_B()
{
    printf("ansi_decode_cmd_B(%d parameters)\n", paramidx);
		paramidx++;
    if (paramidx != 1) {
        printf("ansi_decode_cmd_B() - invalid parameter count = [%d]\n", paramidx);
        return SEQ_ERR;
    }
    /* check the parameter0 is a '2', ie. for '2J' */

    printf("> move cursor down %d lines\n", parameters[0]);
    cursor_y += parameters[0];
    if (cursor_y > (CONSOLE_HEIGHT - 1)) {
        cursor_y = CONSOLE_HEIGHT - 1;
    }
    /* processed entirety of 'J' command */
    return SEQ_ANSI_EXECUTED;
}
