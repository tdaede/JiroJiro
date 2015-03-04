#include <stdio.h>
#include <stdlib.h>

//This section is used for writing some characters to file
int writeFile()
{
	FILE * f;
	const char * str = "This is a simple example showing how to write a string to a file.";
	f = fopen("/tmp/fileTest","wt");
	if (f == NULL)
		return 1;
	fprintf(f, "%s\n", str);
	fflush(f);
	fclose(f);
	return 0;
}

//This section is used for reading some characters from file
int readFile()
{
	FILE * f;
	char * tmpBuf;
	long lSize;
	f = fopen("/tmp/fileTest","rt");
	if (f == NULL)
		return 1;
	fseek (f, 0, SEEK_END);
	lSize = ftell(f);
	printf("Length of file is: %ld\n", lSize);
	fseek (f, 0, SEEK_SET);
	tmpBuf = (char *)malloc(sizeof(char)*lSize);
	size_t n = fread(tmpBuf, 1, lSize, f);
	if (n == lSize)
	{
		printf("Ouput from file is: %s\n", tmpBuf);
	}
	else
	{
		printf("Reading failed %zd\n", n);
		return 1;
	}
	free(tmpBuf);
	fclose(f);
	return 0;
}

int main()
{
	printf("Hello World\n");

	if (writeFile() == 0)
		printf("Writing to file was successful\n");
	else
	{
		printf("Writing to file failed\n");
		exit(1);
	}
	if (readFile() == 0)
		printf("Reading from file was successful\n");
	else
	{
		printf("Reading from file failed\n");
		exit(1);
	}

	return 0;
}
