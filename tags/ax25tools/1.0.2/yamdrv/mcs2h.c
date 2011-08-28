#include <stdio.h>
#include <time.h>
#include <string.h>

int in2hex(char *ptr)
{
	char str[3];
	int val;
	
	memcpy(str, ptr, 2);
	str[2] = '\0';
	
	sscanf(str, "%x", &val);
	
	return val;
}

unsigned char swap(unsigned char c)
{
	unsigned char r = 0;
	int i;
	
	for (i = 0 ; i < 8 ; i++)
	{
		r <<= 1;
		if (c & 1)
			r |= 1;
		c >>= 1;
	}
	return r;
}

int in4hex(char *ptr)
{
	char str[5];
	int val;
	
	memcpy(str, ptr, 4);
	str[4] = '\0';
	
	sscanf(str, "%x", &val);
	
	return val;
}

int main(int ac, char *av[])
{
	int nb, add, type, i;
	int first = 1;
	time_t temps;
	FILE *fptr;
	char buf[256];
	
	if (ac != 3)
	{
		fprintf(stderr, "format : mcs2h 1200|9600 filename\n");
		return 1;
	}
	
	fptr = fopen(av[2], "r");
	if (fptr == NULL)
	{
		fprintf(stderr, "file %s not found\n", av[2]);
		return 1;
	}
	
	time(&temps);
	
	printf( "/*\n"
			" *\n"
			" * File %s converted to h format by mcs2h\n"
			" *\n"
			" * (C) F6FBB 1998\n"
			" *\n"
			" * %s"
			" *\n"
			" */\n\n",
			av[2], ctime(&temps));

	printf("static unsigned char bits_%s[]= {\n", av[1]);

	while (fgets(buf, sizeof(buf), fptr))
	{
		nb   = in2hex(buf+1);
		add  = in4hex(buf+3);
		type = in2hex(buf+7);
		
		if (type != 0)
			continue;
			
		if (first)
			first = 0;
		else
			printf(",\n");

		for (i = 0 ; i < nb ; i++)
		{
			printf("0x%02x%s", swap(in2hex(buf+9+i*2)), (i < (nb-1)) ? "," : "");
		}
	}
	
	printf(" };\n");
	
	fclose(fptr);
	return 0;
}
