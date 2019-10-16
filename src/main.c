#include <stdio.h>
#include <stdlib.h>

#include "as.h"

int main(int argc, char** argv)
{
	AS as;
#if 0
	int i;
#endif
	char* program;
	size_t size;
	FILE* f;
	u16 tmp;
	LABEL* start;

	if(argc != 3) {
		printf("Usage: %s in.s out.bic\n", *argv);
		return 1;
	}

	f = fopen(argv[1], "rb");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	program = malloc(size + 1);
	fread(program, 1, size, f);
	program[size] = 0;
	fclose(f);

	ASInit(&as);
	ASSetSource(&as, program);
	ASCompile(&as);

	start = ASFindLabel(&as, "_START");

	printf("generated %d words\n", as.wr);

#if 0
	printf("program code:\n");
	for(i = 0; i < as.wr; i++) {
		printf("%06o\n", as.code[i]);
	}
#endif
	if(start) {
		printf("entry point: %06o\n", start->addr);
	}

	f = fopen(argv[2], "wb");
	/* header */
	tmp = 1;
	fwrite(&tmp, 2, 1, f);
	/* length */
	tmp = as.wr * 2 + 6;
	fwrite(&tmp, 2, 1, f);
	/* address */
	tmp = as.org;
	fwrite(&tmp, 2, 1, f);
	fwrite(as.code, as.wr, 2, f);
	/* checksum [TODO: implement] */
	tmp = 0;
	fwrite(&tmp, 1, 1, f);

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
	/* checksum [TODO: implement] */
	tmp = 0;
	fwrite(&tmp, 1, 1, f);

	fclose(f);

	ASDestroy(&as);

	free(program);

	return 0;
}
