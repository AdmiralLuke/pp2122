/*
 * Simple error demonstration to demonstrate power of Valgrind.
 * Julian M. Kunkel - 17.04.2008
 */

#include <stdio.h>
#include <stdlib.h>

int*
mistake1(void)
{
	//beim Original war Speicher nur local in Funktion alloziiert
	int* buf = malloc(sizeof(int) * 6);
	buf[0] = 1;
	buf[1] = 1;
	buf[2] = 2;
	buf[3] = 3;
	buf[4] = 4;
	buf[5] = 5;
	return buf;
}

int*
mistake2(void)
{
	int* buf = malloc(sizeof(char) * 4);

	buf[1] = 2; //falscher Index

	return buf;
}

int*
mistake3(void)
{
	/* In dieser Funktion darf kein Speicher direkt allokiert werden. */
	//int  mistake2_ = 0;
	//int* buf       = (int*)&mistake2;
	
	int* buf       = mistake2(); //Funktionen werden nur gecallt mit Klammern

	buf[0] = 3;

	return buf;
}

int*
mistake4(void)
{
	int* buf = malloc(sizeof(char) * 4);

	buf[4] = 4;
	//free(buf); nicht free'n

	return buf;
}

int
main(void)
{

	int* a = mistake1();
	/* Modifizieren Sie die folgende Zeile nicht. */
	int* p[4] = { &mistake1()[1], &mistake2()[1], mistake3(), mistake4() };

	printf("1: %d\n", *p[0]);
	printf("2: %d\n", *p[1]);
	printf("3: %d\n", *p[2]);
	printf("4: %d\n", *(p[3]+4)); //wollen zahl von index 4

	/* Fügen sie hier die korrekten aufrufe von free() ein. */
	free((p[1]-1)); /* Welcher Pointer war das doch gleich? FIXME */
	free((p[0]-1)); 
	free(p[2]);
	free(p[3]);
	return 0;
}
