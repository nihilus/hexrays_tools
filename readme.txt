Milan's useful functions for Hex-Rays decompiler
================================================

New hexrays features:

Assist in creation of new structure definitions / virtual calls detection
===========================================================================

1) use "Reset pointer type" on all variables that you want to scan.

2) Select one of these variables and choose "Scan variable (S)"
  Plugin deals with simple assignments "v1 = this;" automatically. 

3) Again right click on such variable and choose open structure builder.
Ajdust the structure to your likings. 

In Structure builder you can open a list of functions you scanned so far and
functions that were added from virtual function tables. 

Open some of the functions and scan other variables that are of the same
type. Be carefull there is no undo yet.

As you gather more evidence structure builder will show you guessed substructure sizes
and guessed types.

Colliding types have yellow background. Use delete to solve the ambiguity.

With red colour is marked current master offset into structure being created.

Use "*" to change master offset. But you should not need this too often,
because basic situations are detected automatically.

This deals with nested structures:

struct aa 
{
 int foo;
}

struct bb
{
 int foo;
}

struct cc
{
  aa a;
  bb b;
}

void foo(struct *b)
{
  b->foo=4;//while scanning b you should have master offset set to 4
}

void bar(struct *cc)
{
  c->a.foo=5;
  foo(&c->b);
}

substructures
-------------
use "+" and "-" on numeric keyboard to shift parts of the structure. Then use
"pack substruct" to build substructures before finishing the structure.


4) 
Click on some variable in pseudocode and choose "finalize structure"
You will be given a window with "C" declaration of the new structure that you
can tweak by hand and then the new type will by assigned to all variables you
have scanned.

Job done!


Jump directly to virtual function or structure member definition
==================================================================
A class is any structure (A) that has as a pointer into another structure
(A_VT). Where A_VT has a STRUCT comment containing address of the virtual
table and "@0x".



Gives list of structures with given size, with given offset
========================================================================

Right clicking on any number in hexrays view. There are two new items in the menu:
Which structs have this offset?
Which structs have this size?

This will give you list of structures having exact size or this offset (even
in substructures);


Finds structures with same "shape" as is used.
=============================================

Imagine there are structures like this:
00000000 ; ---------------------------------------------------------------------------
00000000 one             struc ; (sizeof=0x8, standard type)
00000000 vfptr           dd ?                    ; offset
00000004 x               dd ?
00000008 one             ends
00000000 ; ---------------------------------------------------------------------------
00000000 two             struc ; (sizeof=0x8)    ; XREF: threer threer
00000000 baseclass_0     one ?
00000008 two             ends
00000008
00000000 ; ---------------------------------------------------------------------------
00000000 two             struc ; (sizeof=0x8)    ; XREF: threer threer
00000000 baseclass_0     one ?
00000008 two             ends
00000000 ; ---------------------------------------------------------------------------
00000000
00000000 three           struc ; (sizeof=0x70)   ; XREF: four::four(void)+29r
00000000                                         ; four::four(void)+2Ar ...
00000000 baseclass_0     two ?
00000008 a               one ?
00000010 b               two ?
00000018 c               dd 20 dup(?)
00000068 d               dd ?
0000006C e               dd ?
00000070 three           ends
00000070
00000000 ; ---------------------------------------------------------------------------
00000000
00000000 four            struc ; (sizeof=0xCC)   ; XREF: four::four(void)+3Cr
00000000                                         ; four::four(void)+43r ...
00000000 baseclass_0     three ?
00000070 a               dd ?
00000074 b               dd ?
00000078 c               dd 20 dup(?)
000000C8 d               dd ?
000000CC four            ends


And you come across a code like this:

void __thiscall four::four(int this)
{
  int v1; // esi@1

  v1 = this;
  *(_DWORD *)(this + 4) = 5;
  *(_DWORD *)(this + 12) = 5;
  *(_DWORD *)(this + 8) = &one::`vftable';
  *(_DWORD *)(this + 20) = 5;
  *(_DWORD *)(this + 16) = &two::`vftable';
  *(_DWORD *)(this + 104) = 0;
  *(_DWORD *)(this + 108) = 0;
  memset((void *)(this + 24), 32, 0x50u);
  *(_DWORD *)v1 = &four::`vftable';
  *(_DWORD *)(v1 + 112) = 0;
  *(_DWORD *)(v1 + 116) = 0;
  *(_DWORD *)(v1 + 200) = 0;
  memset((void *)(v1 + 120), 33, 0x50u);
}

Right click on either "v1" or "this" and choose "recognize shape (T)".
The plugin will show you that the only structure that matches this shape is
"four".

See virtual_calls_4.idb.


convert function to __usercall or __userpurge
===============================================================
Once I dealt with project that mixed Delphi and Visual C++. These two uses
different registers for thiscall/fastcall etc so changing compiler resulted
in ruining analysis. So I made this functionality to deal with it forever.

Just right click on first line of pseudocode and choose "Convert to __usercall".


removes function's return type making it void fnc(...)
===============================================================
Sometimes function does not return any value but hex-rays thinks it does. It
was very tiresome to type "home, y, home, ctrl-shift-left, "void", enter" so I
made Ida do it on one click.

Just right click on first line of pseudocode and choose "Remove return type"
or type 'R'.


Adds Classes on top of structures
===============================================================
menu View / Open Subviews / Classes

Now just list of "interesting" structures inside database along with their
functions.


Automatically zero certain variables in hexrays output
===============================================================
Sometimes in pseudocode there is variable that gets assigned zero and is never
changed. This feature propagates this zero to all places where this variable
was.

The rules are:

"""
zeroizer.cpp
	variable is assigned zero
	variable is never referenced
	warn if variable is on stack

	replace every such variable with zero
"""

Works automatically.

Deals with structures with negative offsets
===========================================
see test2_negative_offsets.idb @004010B0

Without my plugin:
void __cdecl test3(pokusx *p)
{
  *(_DWORD *)&p[-1].array[19996] = 124;
  p[-1].k = 666;
}

With my plugin:
void __cdecl test3(pokusx *p)
{
  container_of(p, 8)->a = 124;
  container_of(p, 8)->cc = 666;
}

Removes function arguments
==========================
Sometimes hexrays thinks that a register is used as an argument for a function
but it isn't so. It was also tiresome to type "home, home, y, ctrl->right,
ctrl-shift-right, delete, enter" So I made this to deal with it.

Just select that argument in the first line of pseudocode and choose "Remove
this argument" or type 'A'.


Assists with changing types of structure members and variables
===============================================================

Imagine you have pseudocode like this.


CASE 1:
{
  type1 v1;
  type2 v2
  v2 = (type2)v1;
}

right click on "v2" in the last line and choose "recast item" v2 will be converted to type "type2";

CASE 2:
you have type declarations:

struct object
{
  struct vtable *v;
}

struct vtable
{
  int fnc1;
  int fnc2;
}


and there is code like this in pseudocode:

struct object * this;
(void (__thiscall *)(object *)this->v->fnc1)(this);

righ clicking on fnc1 and choosing "Recast item" will change type of
"vtable.fnc1" to "void (__thiscall *)(object *)".


Now suppose there is virtual table for object at address 0xABCDEF.

Setting STRUCT comment for "struct vtable" as something containing "@0xABCDEF"
(crucial part is "@0x" ) enables my plugin to navigate to functions located at 0xABCDEF.

But setting this by hand is seldom needed because structure builder takes care of this automatically.



Dealing with delphi classes
===========================
Also there is experimental code for dealing with delphi classes. In menu Edit
/ Structs / Create Delphi class.

The cursor must be on first line of delphi class in ida-view before using
this functionality.

warning: This feature is highly experimental.

New functionality in Structures view
====================================
Use right click in Structures view.

Extract object
--------------
Select few lines in a structure and it will create a new structure off
selected lines.


Unpack object
-------------
Does the opposite of Extract object. It in place unpacks selected structure.

Which structure matches here?
-----------------------------
It will try to guess which structure has the right offsets and types that can
matche selected lines in Structures view.

Add to classes
--------------
Promotes a structure to a class.


Add VT
------
Adds virtual table to a structure. It asks for address to a virtual table.
Then this happens:

00000000 struc_1         struc ; (sizeof=0x10)
00000000 field_0         dd ?
00000004 field_4         dd ?
00000008 field_8         dd ?
0000000C field_C         dd ?
00000010 struc_1         ends

--->

00000000 struc_1         struc ; (sizeof=0x10)
00000000 VT              dd ?                    ; offset
00000004 field_4         dd ?
00000008 field_8         dd ?
0000000C field_C         dd ?
00000010 struc_1         ends
00000010
00000000 ; ---------------------------------------------------------------------------
00000000
00000000 ; @0x004047B0
00000000 struc_1_VT      struc ; (sizeof=0x10)
00000000 sub_40101E      dd ?                    ; sub_40101E
00000004 j___RTC_NumErrors dd ?                  ; j___RTC_NumErrors
00000008 sub_401023      dd ?                    ; sub_401023
0000000C sub_401073      dd ?                    ; sub_401073
00000010 struc_1_VT      ends



the type of "struc_1.VT" is "struc_1_VT*"

When asked for vtableaddress I gave it 0x004047B0.

This enables jumps in hexrays to virtual function.



IDC functionality
=================

my plugins add these idc functions:
reset_alt_calls
---------------
I messed up my idb database and this was a cure:

static void reset_alt_calls(ea_t ea)
{
	static const char * nname;
	if ( ph.id == PLFM_MIPS )
		nname = "$ mips";
	else if ( ph.id == PLFM_ARM )
		nname = " $arm";
	else
		nname = "$ vmm functions";
	netnode n(nname);
	for(nodeidx_t idx= n.alt1st(); idx != BADNODE; idx = n.altnxt(idx))
	{
		if(n.altval(idx)-1 == ea)
		{
			n.altdel(idx);
			noUsed(idx);                 // reanalyze the current instruction
			recalc_spd(idx);
		}	
		//n.altdel_all(atag);
	}
}


structs_of_size(num)
-------------------
Just calls "Which structure has size num?".

Graph of structures inside structures.
======================================
structures_graph_0.png, see structures_graph.png
menu View/graphs/show structures graph


Milan Bohacek
