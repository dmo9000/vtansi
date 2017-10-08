#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

extern int errno;

#define CHUNK_SIZE	128
#define BUF_SIZE 		4096
#define MAX_ANSI		64				/* maximum length allowed for an ANSI sequence */

static char filebuf[4096];
static char ansibuf[MAX_ANSI];

#define SEQ_ERR				0	
#define SEQ_NONE			1	
#define SEQ_ESC_1B		2	
#define SEQ_ANSI_5B		3		

static char *states[] = {
													"SEQ_ERR",
													"SEQ_NONE",
													"SEQ_ESC_1B",
													"SEQ_ANSI_5B"
											};

char ansi_mode = 0;
char last_ansi_mode = 0;
int ansioffset = 0;

int decode0(char);
int decode1(char);
int decode2(char);
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
            printf("0x%04x: error in state, ansi_mode = [%s], last_ansi_mode = [%s], character = 0x%02x\n", offset, 
													(const char *) ansi_state(ansi_mode), (const char *) ansi_state(last_ansi_mode), last_c);
						printf("ANSIBUF[0x%02x]: ", ansioffset);
						for (i = 0; i < ansioffset; i++) {
								printf("[0x%02x]", ansibuf[i]);
								} 
						printf("\n");
            exit(1);
            break;
        case SEQ_NONE:
						last_ansi_mode = ansi_mode;
            ansi_mode = decode0(c);
            break;
        case SEQ_ESC_1B:
						last_ansi_mode = ansi_mode;
            ansi_mode = decode1(c);
            break;
        case SEQ_ANSI_5B:
						last_ansi_mode = ansi_mode;
            ansi_mode = decode2(c);
            break;
        default:
            printf("0x%04x: unhandled ansi_mode = 0x%02x\n", offset, ansi_mode);
            exit(0);
            break;
        }
        offset ++;
    }

    printf("\n");
    printf("[reached end of buffer]\n");
    fclose(ansfile);
    exit (0);
}

int decode0(char c)
{

		printf("decode0(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
    if (ansi_mode == SEQ_NONE && c == 0x1b) {
				ansioffset = 0;
        memset(&ansibuf, 0, MAX_ANSI);
				ansibuf[ansioffset] = c;
				ansioffset++;
        return SEQ_ESC_1B;
    }

    return SEQ_NONE;
}

int decode1(char c)
{
		printf("decode1(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
		if (ansi_mode == SEQ_ESC_1B && c == '[') {
				ansibuf[ansioffset] = c;
				ansioffset++;
				return SEQ_ANSI_5B;
				}

    return SEQ_ERR;
}

int decode2(char c)
{
		printf("decode2(0x%02x, ansi_mode = %s, last_ansi_mode = %s)\n", c, ansi_state(ansi_mode), ansi_state(last_ansi_mode));
		if (ansi_mode == SEQ_ANSI_5B) {
				if (isdigit(c)) {
						return SEQ_ERR;
						}
				return SEQ_ERR;
				}

    return SEQ_ERR;
}




const char *ansi_state(int s)
{

		return (const char *) states[s];
	
}



