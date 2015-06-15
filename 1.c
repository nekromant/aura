/*       The  following program inserts 24 items in to a hash table, then prints
       some of them, using the re-entrant version.
       */

	   #include <stdio.h>
	   #define __USE_GNU
	   #include <search.h>

	   char *data[] = { "alpha", "bravo", "charlie", "delta",
		"echo", "foxtrot", "golf", "hotel", "india", "juliet",
		"kilo", "lima", "mike", "november", "oscar", "papa",
		"quebec", "romeo", "sierra", "tango", "uniform",
		"victor", "whisky", "x-ray", "yankee", "zulu"
	   };

	   int main() {
	     struct hsearch_data htab;
	     ENTRY e, *ep;
	     int i;
	     int r;

	     memset((void *)&htab, 0, sizeof(htab));

	     /* starting with small table, and letting it grow does not work */
	     r = hcreate_r(30,&htab);
	     if (r == 0) {
	       fprintf(stderr, "create failed\n");
	       exit(1);
	     }
	     for (i = 0; i < 24; i++) {
		 e.key = data[i];
		 /* data is just an integer, instead of a
		    pointer to something */
		 e.data = (char *)i;
		 r = hsearch_r(e, ENTER, &ep, &htab);
		 /* there should be no failures */
		 if (!r) {
		   fprintf(stderr, "entry failed\n");
		   exit(1);
		 }
	     }
	     for (i = 22; i < 26; i++) {
		 /* print two entries from the table, and
		    show that two are not in the table */
		 e.key = data[i];
		 r = hsearch_r(e, FIND, &ep, &htab);
		 printf("%9.9s -> %9.9s:%d\n", e.key,
			r ? ep->key : "NULL",
			r ? (int)(ep->data) : 0);
	     }
	     return 0;
	   }

