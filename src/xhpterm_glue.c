#include "config.h"
#include <stdio.h>

void Logit (int typ, char *ptr, size_t len, bool special_dc1)
{ /*Logit*/

  extern int
    logging;

  if (!logging)
    return;

  if (log_type & LOG_PREFIX)
    {
      if (typ == LOG_INPUT)
	fprintf (logFd, "in:  ");

      else if (typ == LOG_OUTPUT)
	fprintf (logFd, "out: ");

      else 
	fprintf (logFd, "???: ");
    } 

  while (len--)
    {
      if (((int) *ptr < 32) || ((int) *ptr == 127))
	{
	  int index = (int) *ptr;
	  if (index == 127)
	    index = 33;
	  fprintf (logFd, "%s", asc_logvalue[index]);
	  if (index == ASC_LF)
	    putc ('\n', logFd);
	}
      else
	putc ((int)*ptr, logFd);

      if (special_dc1 && (*ptr == ASC_DC1))	/* Ugh */
	putc ('\n', logFd);

      ++ptr;
    }

  putc ('\n', logFd);

  fflush (logFd);

} /* Logit */
