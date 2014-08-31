#include <stdio.h>
#include <memory.h>

class one
{
    int x;
 public:
 one():x(5)
 {
 }

 virtual ~one()
 {
  printf("destruct one\n");

 }

 virtual int boo(){return 1;}
};

class two: public one
{
 public:
 virtual int boo(){return 2;}
 virtual int foo(){return 22;}

 virtual ~two()
 {
  printf("destruct two\n");
 }

};


class three: public two
{
public:
 one a;
 two b;
 int c[20];
 int d;
 int e;

 public:
 three():d(0),e(0){memset(c,32,20*4);}

 virtual ~three()
 {
  printf("destruct three\n");

 }
 virtual int boo(){return 3;}
 virtual int foo(){return 33;}
 virtual int goo(){return 333;}
};


class four: public three
{
public:
 int a;
 int b;
 int c[20];
 int d;
 four():a(0),b(0),d(0)
 {
	memset(c,33,20*4);
 }


 virtual ~four()
 {
  printf("destruct four\n");
 }
 virtual int boo(){return 4;}
 virtual int foo(){return 44;}
 virtual int goo(){return 444;}
};

class five: public four
{

};


struct base {
   int value;
   virtual void foo() { printf("%d\n", value); }
   virtual void bar() { printf("%d\n", value); }
};

struct offset {
   char space[10];
};

struct derived : offset, base {
   int dvalue;
   virtual void foo() { printf("%d\n", value); }
};


void main()
{

  five  * cls3 = new five();
  printf("%d\n",cls3->boo());
  printf("%d\n",cls3->foo());
  printf("%d\n",cls3->goo());
  
  delete cls3;

  derived * d = new derived();
  d->dvalue = 5;
  d->value = 5;
  d->space[0] = 0;
  d->space[5] = 5;
  d->space[9] = 9;
  d->foo();
  d->bar();
}
