#include "utility.h"
#include <msp430.h>

void Utility_delay(unsigned short int loops) {
	while (loops--) {
		__nop();
	}
}

/**
 * convert the hex value supplied in s into a 32bit integer, terminating on count or \0
 */
unsigned long int Utility_hexToInt(const char* s, int count) {
	unsigned long int ret = 0;
	char c;
	while ((c = *s++) != '\0' && count--) {
		int n = 0;
		if('0' <= c && c <= '9') { n = c-'0'; }
		else if('a' <= c && c <= 'f') { n = 10 + c - 'a'; }
		else if('A' <= c && c <= 'F') { n = 10 + c - 'A'; }
		ret = n + (ret << 4);
	}
	return ret;
}

static const char hexchars[] = "0123456789ABCDEF";

/**
 * translate count bytes from ptr into ascii hex and store them in the dst array.
 */
void Utility_intToHex(char* dst, const void* ptr, int count) {
	int i;
	const unsigned char* cptr = (const unsigned char*)ptr;
	unsigned int j = (count << 1);
	dst[j] = 0;
	for (i = 0; i < count; i++) {
		dst[--j] = hexchars[((cptr[i]) & 0xf)];
		dst[--j] = hexchars[((cptr[i] >> 4) & 0xf)];
	}
}

void Utility_reverse(char * str, int sz) {
	char * j;
	int c;
	j = str + sz - 1;
	while(str < j) {
		c = *str;
		*str++ = *j;
		*j-- = c;
	}
}

int Utility_intToAPadded(char * ptr, int value, int radix, int padding) {
	int orig = value;
	int digit;
	char * dst = ptr;
	int sz = 0;
	if (value < 0) {
		value = -value;
	}
	while (value) {
		digit = value % radix;
		*dst = hexchars[digit];
		dst++;
		value = value / radix;
		sz++;
	}
	padding = padding - sz;
	while (padding-- > 0) {
		*dst = '0';
		dst++;
		sz++;
	}
	if (orig < 0) {
		*dst = '-';
		dst++;
		sz++;
	}
	*dst = '\x00';
	Utility_reverse(ptr, sz);
	return sz;
}

int Utility_aToInt(char * ptr) {
	int idx;
	idx = 0;
	int valueisnegative = 0;
	int result_value = 0;
	while (ptr[idx] == '\t' || ptr[idx] == ' ') {
		idx++;
	}
	if (ptr[idx] == '-') {
		valueisnegative = 1;
		idx++;
	} else if (ptr[idx] == '+') {
		idx++;
	}
	while (ptr[idx] >= '0' && ptr[idx] <= '9') {
		result_value = result_value * 10;
		result_value += (ptr[idx] - '0');
		idx++;
	}
	if (valueisnegative) {
		result_value = -(result_value);
	}
	return result_value;
}
