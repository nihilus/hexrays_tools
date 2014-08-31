#pragma once
struct matched_structs
{
	intvec_t idcka;
	int choosed;	
	static uint32 idaapi sizer(void *obj);
	static char * idaapi get_type_line(void *obj, uint32 n, char *buf);

	int choose(char * title, int width = 40)
	{
		choosed = ::choose((void *)this, width, sizer, get_type_line, title );
		return choosed;
	}
};


static const int widths_matched_structs_with_offsets[] = { 40, 32 };

struct matched_structs_with_offsets
{
	asize_t offset;
	intvec_t idcka;
	int choosed;
	static uint32 idaapi sizer(void *obj);
	static void idaapi get_type_line(void *obj,uint32 n,char * const *arrptr);

	int choose(char * title, int width = 40)
	{
		choosed = ::choose2((void *)this, 2, widths_matched_structs_with_offsets, sizer, get_type_line, title );
		return choosed;
	}
};


static const int widths[] = { CHCOL_HEX|8, 32, 32 };

struct function_list
{	
	eavec_t functions;
	int choosed;

// column widths
	

	// function that returns number of lines in the list
	static uint32 idaapi sizer(void *obj);
	static void idaapi get_line(void *obj,uint32 n,char * const *arrptr);

	int choose(char * title)
	{	
		choosed = ::choose2((void *)this, 3, widths, sizer, get_line, title);
		return choosed;
	}


	void open_pseudocode()
	{
		if(choosed>0)
			::open_pseudocode(functions[choosed-1], -1);
	}

	void open_assembler()
	{
		
		if(choosed>0)
			::jumpto(functions[choosed-1], -1);
	}
};