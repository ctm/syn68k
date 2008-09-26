#ifndef _PROCESS_H_INCLUDED_
#define _PROCESS_H_INCLUDED_

#include <stdio.h>

extern int process_template (FILE *fp, FILE *header_fp, const template_t *t,
			     const char *make, int swapop_p);
extern int count_operands (const char *s);

#endif  /* !_PROCESS_H_INCLUDED_ */
