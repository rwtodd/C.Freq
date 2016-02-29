# freq
Character/Byte frequency counter

This is a C clone of a Go program (https://github.com/robpike/freq), which itself was a 
clone of an old C program.  The Go program works on full unicode, whereas the original 
only worked on bytes/ASCII.

I wanted to see how painful it would be to make a C version that worked on unicode
like the Go version __without__ using external libs like ICU.  Turns out... it's 
not so bad if you are on Linux or OS X, where `wchar_t` is 4 bytes.  On Windows--even 
cygwin--you get a 2-byte wchar_t. The upside is that the code still compiles and works, 
but you only get the 16-bit BMP style of unicode, and any combining chars will just 
be in the list as-is.

I'm sad to say that as of 2016, C11 unicode still isn't supported correctly (as fas as 
I understand it) by compilers.  On cygwin you get \_\_STD\_ISO\_10646\_\_ defined
even though `wchar_t` is too small.  Cygwin and OS X clang both seem to leave out
the `uchar.h` header, but at least in both cases \_\_STD\_UTF\_32\_\_ was not defined.
