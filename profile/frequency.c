#include "bucket.h"
#include "frequency.h"
#include "readprofile.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static void
fill_buckets ()
{
  int i;
  for (i = 65535; i >= 0; i--)
    bucket[bucket_map[i]].count += instruction_count[i];
}


/* qsort helper function.  Place larger counts first. */
static int
compare_buckets (const void *b1, const void *b2)
{
  int diff = ((const Bucket *)b2)->count - ((const Bucket *)b1)->count;
  if (diff != 0)
    return diff;

  /* Sort alphabetically if the counts are the same. */
  return strcmp (((const Bucket *)b1)->name, ((const Bucket *)b2)->name);
}


void
generate_frequency_report ()
{
  int i;

  fill_buckets ();
  qsort (bucket, num_buckets, sizeof bucket[0], compare_buckets);

  puts ("count\topcode\n"
	"-------\t-----------");
  for (i = 0; i < num_buckets && bucket[i].count != 0; i++)
    printf ("%lu\t%s\n", bucket[i].count, bucket[i].name);
}
