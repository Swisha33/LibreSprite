/* LibreSprite
 * Copyright (C) 2026 LibreSprite contributors
 *
 * This file is released under the terms of the MIT license.
 * Read LICENSE.txt for more information.
 *
 * Range-safe <ctype.h> functions for the PS Vita build.
 *
 * The UI passes Unicode code points and key codes to tolower(), isspace()
 * and friends. glibc tolerates values outside [-1, 255]; newlib indexes
 * its ctype table directly and reads out of bounds, which crashes the
 * Vita. These definitions take precedence over the newlib archive
 * members and give the "C" locale results for ASCII, while any other
 * value is simply not classified (and returned unchanged by
 * tolower/toupper).
 */

#ifdef __vita__

static int in_ascii(int c) { return c >= 0 && c < 128; }

int isupper(int c)  { return in_ascii(c) && c >= 'A' && c <= 'Z'; }
int islower(int c)  { return in_ascii(c) && c >= 'a' && c <= 'z'; }
int isalpha(int c)  { return isupper(c) || islower(c); }
int isdigit(int c)  { return in_ascii(c) && c >= '0' && c <= '9'; }
int isalnum(int c)  { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || (in_ascii(c) && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))); }
int isspace(int c)  { return c == ' ' || (c >= '\t' && c <= '\r'); }
int isblank(int c)  { return c == ' ' || c == '\t'; }
int iscntrl(int c)  { return in_ascii(c) && (c < 32 || c == 127); }
int isprint(int c)  { return c >= 32 && c < 127; }
int isgraph(int c)  { return c > 32 && c < 127; }
int ispunct(int c)  { return isgraph(c) && !isalnum(c); }
int tolower(int c)  { return isupper(c) ? c - 'A' + 'a' : c; }
int toupper(int c)  { return islower(c) ? c - 'a' + 'A' : c; }

#endif
