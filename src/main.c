#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "as.h"

#define	CHECKSUM16(x)	((u8) (x) + (u8) ((x) >> 8))

int cmp(const void* a, const void* b)
{
	LABEL* l1 = *(LABEL**) a;
	LABEL* l2 = *(LABEL**) b;

	return l1->addr - l2->addr;
}

int main(int argc, char** argv)
{
	AS as;
	int i;
	char* program;
	size_t size;
	FILE* f;
	u16 tmp;
	u8 checksum;
	LABEL* start;

	int print_labels = 0;

	const char* execfn = *argv;

	if(argc > 1) {
		if(!strcmp("-p", argv[1])) {
			print_labels = 1;
			argc--;
			argv++;
		}
	}

	if(argc != 3) {
		printf("Usage: %s in.s out.bic\n", execfn);
		return 1;
	}

	f = fopen(argv[1], "rb");
	if(!f) {
		printf("ERROR: cannot open file \"%s\"\n", argv[1]);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	program = malloc(size + 1);
	if(!program) {
		printf("ERROR: cannot allocate memory\n");
		return 1;
	}
	fread(program, 1, size, f);
	program[size] = 0;
	fclose(f);

	ASInit(&as);
	ASSetSource(&as, program);
	ASCompile(&as);

	start = ASFindLabel(&as, "_START");

	printf("generated %d words\n", as.wr);

	if(start) {
		printf("entry point: %06o\n", start->addr);
	}

	f = fopen(argv[2], "wb");
	if(!f) {
		printf("ERROR: cannot open file \"%s\"\n", argv[2]);
		return 1;
	}
	/* header */
	tmp = 1;
	fwrite(&tmp, 2, 1, f);
	/* length */
	tmp = as.wr * 2 + 6;
	checksum = 1 + CHECKSUM16(tmp);
	fwrite(&tmp, 2, 1, f);
	/* address */
	tmp = as.org;
	checksum += CHECKSUM16(tmp);
	fwrite(&tmp, 2, 1, f);
	fwrite(as.code, as.wr, 2, f);
	for(i = 0; i < as.wr; i++) {
		checksum += CHECKSUM16(as.code[i]);
	}
	checksum = -checksum;
	fwrite(&checksum, 1, 1, f);

	/* header */
	tmp = 1;
	fwrite(&tmp, 2, 1, f);
	/* length */
	tmp = 6;
	fwrite(&tmp, 2, 1, f);
	/* address */
	if(start != 0) {
		tmp = start->addr;
		fwrite(&tmp, 2, 1, f);
	} else {
		tmp = 000001;
		fwrite(&tmp, 2, 1, f);
	}
	checksum = -(1 + 6 + CHECKSUM16(tmp));
	fwrite(&checksum, 1, 1, f);

	fclose(f);

	if(print_labels) {
		int cnt = 0;
		int i = 0;
		LABEL* lbl = as.labels;
		LABEL** sorted;
		while(lbl) {
			cnt++;
			lbl = lbl->next;
		}

		sorted = (LABEL**) malloc(cnt * sizeof(LABEL*));
		if(!sorted) {
			printf("ERROR: cannot allocate memory\n");
			return 0;
		}

		lbl = as.labels;
		while(lbl) {
			sorted[i++] = lbl;
			lbl = lbl->next;
		}

		qsort(sorted, cnt, sizeof(LABEL*), cmp);

		for(i = 0; i < cnt; i++) {
			lbl = sorted[i];
			printf("%-32s = %06o\n", lbl->name, lbl->addr);
		}

		free(sorted);
	}

	ASDestroy(&as);

	free(program);

	return 0;
}
