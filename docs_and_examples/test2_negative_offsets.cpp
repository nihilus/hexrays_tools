#include <stdio.h>

struct pokus
{
	char ch;
	int i;
	char array[20000];
	int k;
};


struct pokusy
{
  int a;
  int cc;
  struct pokus b;
};

struct pokus_1
{
  int a;
  pokus * p;
  pokus p2;
};



void test2(pokus * p)
{
	p->ch = 1;
	p->i=2;
	pokus * q = p;
	q->array[0] = 10;
	for(int i=0; i<20; i++)
	{
		q->array[i]=i;
	}
	q->k=2048;
}

void test3(pokus * p)
{
   pokusy * b = (pokusy *)((char*)p-8);
   b->a = 124;
   b->cc = 666;
}

void test5(pokus * p)
{
   int * c = (int *)((char*)p-4);
   *c = 666;

   pokusy * b = (pokusy *)((char*)p-8);
   b->a = 124;

}

void test4(pokus_1 * a)
{
 a->p->i = 2;
 a->p2.ch = 1;
 a->a = 3;
}

pokus * test6()
{
	static pokus a;
	return &a;
}

void testing(int a)
{
 int arg1 = a;
 a = sprintf((char *)&arg1, "%d", a);
 printf("%s", &arg1, &a);
}

void main()
{
	pokus * bbb;
	pokus p;
 	pokusy pp;
	p.ch = 1;
	p.i=2;
	p.array[0] = 10;
	for(int i=0; i<20; i++)
	{
		p.array[i]=i;	
	}
	bbb=&p;
	test2(bbb);

	pokus * b = &pp.b;
	bbb = b;
	test6()->i=314;
        test3(b);
        test5(b);
	printf("b->a: %d\n", pp.b);
}
