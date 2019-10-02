#include <stdio.h>
#include <stdlib.h>

#include "as.h"

int main(int argc, char** argv)
{
	AS as;
	int i;
	char* program;
	size_t size;
	FILE* f = fopen("test.s", "rb");
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

	printf("generated %d words\n", as.wr);

	printf("program code:\n");
	for(i = 0; i < as.wr; i++) {
		printf("%06o\n", as.code[i]);
	}

	ASDestroy(&as);

	free(program);

	return 0;
}
