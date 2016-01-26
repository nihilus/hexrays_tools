#include <hexrays.hpp>
#include "defs.h"
#include "ripped.h"


//tinfo_t t_char = "2";
//tinfo_t t_byte = "\x11";

signed int __cdecl log2ceil(__int64 a1)
{
  int v1; // edx@1
  signed int result; // eax@2
  int v3; // edx@4
  signed int v4; // ecx@4

  v1 = HIDWORD(a1);
  if ( HIDWORD(a1) )
  { 
    result = 32;
  }
  else
  {
    v1 = a1;
    result = 0;
  }
  v3 = v1 - 1;
  v4 = 31;
  while ( !((1 << v4) & v3) )
  {
    --v4;
    if ( v4 < 0 )
      return result;
  }
  result += v4 + 1;
  return result;
}


#if IDA_SDK_VERSION >= 630
/*C<qstring  __cdecl (typestring *a2, int offset), 0x17035E90> create_field_name;*/
//v6 0x170C96D0 ... v61 0x17034C50
C<typestring  __cdecl (const type_t *), 0x17034C50> dummy_plist_for;
//V6 0x170C95C0 ... V61 17034B00
C<typestring __cdecl (int a2), 0x17034B00> make_dt_;
#endif

tinfo_t make_dt(int a2)
{
	
	type_t a[12];
	type_t * konec = set_dt((type_t*)&a, a2);
	//tinfo_t s(a, konec-a);
	tinfo_t s(konec-a);
	return s;
}

int make_dtname(type_t * vysl, int max, const char *name)
{
	type_t a[12];
	memset(a, 0, sizeof(a));
	int len = strlen(name);
	type_t * k = set_dt(a, len);
	int len1 = k-a;
	int komplet = len1 + len;
	if (komplet < max)
	{
		memcpy(vysl, a, len1);
		memcpy(vysl + len1, name, len);
		vysl[komplet] = 0;
	}
	else return -1;
	return komplet;
}