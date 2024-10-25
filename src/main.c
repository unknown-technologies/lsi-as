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

void print_usage(const char* execfn, int detail)
{
	printf("Usage: %s [-q] [-p] in.s out.bic\n", execfn);
	if(detail) {
		printf("\n"
			"OPTIONS\n"
			"    -h      Show this help message and exit\n"
			"    -p      Print all defined labels/constants on exit\n"
			"    -q      Quiet mode, do not print entry point and size\n");
	}
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
	int quiet = 0;

	const char* execfn = *argv;

	const char* infilename = NULL;
	const char* outfilename = NULL;

	argc--;
	argv++;

	for(; argc; argc--, argv++) {
		if(!strcmp("-p", *argv)) {
			print_labels = 1;
		} else if(!strcmp("-q", *argv)) {
			quiet = 1;
		} else if(!strcmp("-h", *argv)) {
			print_usage(execfn, 1);
			return 0;
		} else if(!strcmp("-i", *argv)) {
			if(argc > 1) {
				infilename = argv[1];
			} else {
				printf("Missing argument: input file\n");
				return 1;
			}
			argc--;
			argv++;
		} else if(!strcmp("-o", *argv)) {
			if(argc > 1) {
				outfilename = argv[1];
			} else {
				printf("Missing argument: output file\n");
				return 1;
			}
			argc--;
			argv++;
		} else if(**argv != '-') {
			break;
		} else {
			printf("Unknown option: %s\n", *argv);
			return 1;
		}
	}

	if(argc > 0) {
		infilename = argv[0];
	}

	if(argc > 1) {
		outfilename = argv[1];
	}

	if(argc > 2) {
		print_usage(execfn, 0);
		return 1;
	}

	if(!infilename) {
		printf("ERROR: no input file\n");
		return 1;
	}

	if(!outfilename) {
		printf("ERROR: no output file\n");
		return 1;
	}

	f = fopen(infilename, "rb");
	if(!f) {
		printf("ERROR: cannot open file \"%s\"\n", infilename);
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

	if(ASIsError(&as)) {
		ASDestroy(&as);
		free(program);
		return 1;
	}

	start = ASFindLabel(&as, "_START");

	if(!quiet) {
		printf("generated %d words\n", as.wr);
	}

	if(start && !quiet) {
		printf("entry point: %06o\n", start->addr);
	}

	f = fopen(outfilename, "wb");
	if(!f) {
		printf("ERROR: cannot open file \"%s\"\n", outfilename);
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
