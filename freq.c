#include<wchar.h>
#include<wctype.h>
#include<stdbool.h>
#include<string.h>
#include<locale.h>
#include<stdio.h>
#include<unistd.h>
#include<stdint.h>
#include<stdlib.h>
#include<fcntl.h>
#include<inttypes.h>

/* Issue some warnings if the full C11
 * and UTF-32 sizes aren't available.
 * Note on OS X it seems wchar_t is 4-bytes
 * but the __STDC_ISO_10646__ isn't there.
 */
#ifndef __STDC_ISO_10646__
#warning WCHAR_T may not be wide enough. __STDC_ISO_10646__ is not defined.
#endif

#ifdef __STD_UTF_32__
#include<uchar.h>
#else
#warning Compiler does not have char32_t support. Hacking around it.
typedef wchar_t char32_t;
#define mbrtoc32 mbrtowc
#endif

/* -------------------------------------- */

/* emalloc either allocates and zero-fills,  or
 * it halts the program.  This keeps callers from
 * having to check the return value.
 */
static void *emalloc(size_t sz) 
{
    void *m = malloc(sz);
    if (m == NULL) {
	fputs("Could not allocate memory!\n", stderr);
	exit(1);
    }
    memset(m, 0, sz);
    return m;
}

/* I'm using the same construct as the Go
 * program: a lazily-allocated set of arrays.
 * counts[01][02][03] will retrieve the count
 * for unicode codepoint 0x010203.
 */
uint64_t **counts[256];

/* inc increases the count of unicode character 'c',
 * allocating storage as necessary.
 */
static void inc(char32_t c)
{
    size_t b1 = (c >> 16) & 0xff;
    if (counts[b1] == NULL) {
	counts[b1] = emalloc(256 * sizeof(uint64_t *));
    }
    size_t b2 = (c >> 8) & 0xff;
    if (counts[b1][b2] == NULL) {
	counts[b1][b2] = emalloc(256 * sizeof(uint64_t));
    }
    size_t b3 = c & 0xff;
    counts[b1][b2][b3] += 1;
}

/* buffer is a place to store the input */
char buffer[4096];
#define BSZ sizeof(buffer)

/* a type that can be either parse_buffer or parse_buffer_bytes */
typedef int (*parser) (size_t);

/* parse_buffer_bytes: read all the bytes, collecting
 * counts.
 */
static int parse_buffer_bytes(size_t avail)
{
    const char *loc = buffer;
    const char *const end = loc + avail;

    while (loc != end) {
	inc((char32_t) (*loc) & 0xff);
	++loc;
    }
    return 0;
}

/* parse_buffer: read all the unicode characters,
 * and collect counts
 */
static int parse_buffer(size_t avail)
{
    char32_t c;
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));
    const char *loc = buffer;

    size_t width;
    while (avail > 0) {
	width = mbrtoc32(&c, loc, avail, &mbs);
	if (width == (size_t) - 2) {
	    /* incomplete unicode, return num chars left */
	    return avail;
	} else if (width == (size_t) - 1) {
	    /* error exit */
	    return -1;
	}
	inc(c);
	loc += width;
	avail -= width;
    }
    return 0;
}

/* print_stats: list all the collected counts. */
static void print_stats(bool use_bytes)
{
    char fmt[25];
    sprintf(fmt, "0x%%0%dX '%%lc':\t%%%s\n", use_bytes ? 2 : 6, PRIu64);

    for (size_t i = 0; i < 256; ++i) {
	if (counts[i] == NULL)
	    continue;

	for (size_t j = 0; j < 256; ++j) {
	    if (counts[i][j] == NULL)
		continue;

            /* set up the character, except for the final 8 bits 
             * which I'll fix up in the innermost loop.
             */
	    char32_t c = ((char32_t) i) << 16 | ((char32_t) j) << 8;

	    for (size_t k = 0; k < 256; ++k) {
		uint64_t total = counts[i][j][k];
		if (total > 0) {
                    c = (c & 0xffff00) | (char32_t)k;
		    char32_t c2 = iswgraph(c) ? c : '-';
		    printf(fmt, c, c2, total);
		}
	    }

	}

    }
}

static void usage(void)
{
    fprintf(stderr, "Usage: freq [-b] [file]\n");
    fprintf(stderr, "  -b   Count bytes instead of unicode chars\n");
    exit(2);
}

int main(int argc, char **argv)
{
    /* setup */
    setlocale(LC_ALL, "");
    bool use_bytes = false;
    memset(counts, 0, sizeof(counts));

    /* parse cmd line */
    int opt;
    while ((opt = getopt(argc, argv, "hb")) != -1) {
	switch (opt) {
	case 'b':
	    use_bytes = true;
	    break;
	case 'h':
	    usage();
	    break;
	}
    }

    /* default to stdin if no filename */
    int fd = 0; 
    if( optind < argc ) {
	if((fd = open(argv[optind],O_RDONLY)) < 0) {
		perror("input error");
		exit(1);
        }
    }

    /* process input */
    parser p = use_bytes ? parse_buffer_bytes : parse_buffer;
    int left = 0;
    size_t sz = 0;
    while ((sz = read(fd, buffer + left, BSZ - left)) > 0) {
	left = p(left + sz);
	if (left < 0) {
	    fprintf(stderr, "Error parsing characters %s\n",
		    use_bytes ? "" : "(not UTF-8? use -b option)");
	    break;
	}
	memcpy(buffer, buffer + BSZ - left, left);
    }
    close(fd);

    /* report on any errors */
    if (left != 0 || sz != 0) {
	fprintf(stderr, "Errors while reading file!\n");
	return 1;
    }
    print_stats(use_bytes);
    return 0;
}
