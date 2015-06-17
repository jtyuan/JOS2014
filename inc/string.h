#ifndef JOS_INC_STRING_H
#define JOS_INC_STRING_H

#include <inc/types.h>

int	strlen(const char *s);
int	strnlen(const char *s, size_t size);
char *	strcpy(char *dst, const char *src);
char *	strncpy(char *dst, const char *src, size_t size);
char *	strcat(char *dst, const char *src);
size_t	strlcpy(char *dst, const char *src, size_t size);
int	strcmp(const char *s1, const char *s2);
int	strncmp(const char *s1, const char *s2, size_t size);
char *	strchr(const char *s, char c);
char *	strrchr(const char *s, char ch);			// Lab 7
char *	strfind(const char *s, char c);
char *	strrev(char *str);					// Lab 7

void *	memset(void *dst, int c, size_t len);
void *	memcpy(void *dst, const void *src, size_t len);
void *	memmove(void *dst, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);
void *	memfind(const void *s, int c, size_t len);

long	strtol(const char *s, char **endptr, int base);

// Lab 7
int 	hexval(char c);
int 	isdigit(int c);
int 	isxdigit(int c);
char *	itoa(int num, char *str, int base);
int	atoi(const char *num);

#endif /* not JOS_INC_STRING_H */
