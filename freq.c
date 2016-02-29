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

/* the counters. FIXME only ASCII range for now. */
uint64_t counts[256];

/* the buffer we use for input */
char buffer[1024];
#define BSZ sizeof(buffer)

/* a type that can be either parse_buffer or parse_buffer_bytes */
typedef int (*parser) (size_t);

/* parse_line_bytes: read all the bytes, collecting
 * counts.
 */
static int parse_buffer_bytes(size_t avail)
{
    const char *loc = buffer;
    const char *const end = loc + avail;

    while (loc != end) {
	counts[(size_t) (*loc & 0xff)]++;
	++loc;
    }
    return 0;
}

/* parse_line: read all the unicode characters, 
 * and collect counts 
 */
static int parse_buffer(size_t avail) {
    char32_t c;
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));
    const char *loc = buffer;

    size_t width;
    while (avail > 0) {
	width = mbrtoc32(&c, loc, avail, &mbs);
	if( width == (size_t)-2 ) {
	    /* incomplete unicode, return num chars left */
            return avail;  
       	} else if (width == (size_t)-1) {
	    /* error exit */
	    return -1;
        }

	if (c < 256) {  /* FIXME temporary check */
	    counts[(size_t) c]++;
	}
	loc += width;
	avail -= width;
    }
    return 0;
}

/* print_stats: list all the collected counts. */
static void print_stats(bool use_bytes)
{
    char fmt[25];
    sprintf(fmt, "0x%%0%dX\t'%%lc':\t%%Lu\n", use_bytes ? 2 : 6);
    for (int i = 0; i < 256; ++i) {
	uint64_t t = counts[i];
	if (t == 0)
	    continue;
	char32_t c = (wchar_t) i;
	char32_t c2 = iswgraph(c) ? c : '-';
	printf(fmt, c, c2, t);
    }
}

static void usage(void) {
     fprintf(stderr,"Usage: freq [-b] [file ...]\n");
     fprintf(stderr,"  -b   Count bytes instead of unicode chars\n");
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
    while((opt=getopt(argc,argv,"hb")) != -1) {
      switch(opt) {
      case 'b':
 	 use_bytes = true;
 	 break;
      case 'h':
	 usage();
	 break;
      }
    }

    /* process input */
    parser p = use_bytes?parse_buffer_bytes:parse_buffer;
    int left = 0;
    size_t sz = 0;
    while ((sz=read(0,buffer+left,BSZ-left)) > 0) {
	    left = p(left+sz);
	    if(left < 0) { 
		fprintf(stderr, "Error parsing characters %s\n", 
			use_bytes?"":"(not UTF-8? use -b option)");
		break;
	    } 
	    memcpy(buffer, buffer+BSZ-left, left);
    }

    /* report on any errors */
    if(left != 0 || sz != 0) {
        fprintf(stderr,"Errors while reading file!\n");
	return 1;
    }

    print_stats(use_bytes);
    return 0;
}
