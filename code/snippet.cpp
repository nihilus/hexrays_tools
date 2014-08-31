void convert_test(cexpr_t *e)
{
	// sub -> num , var => helper object
	if (e->op == cot_sub)
	{
		cexpr_t *var = e->x;
		if (var->op == cot_cast)
			var = var->x;
		cexpr_t * num = e->y;

		if(var->op == cot_var && num->op == cot_num)
		{
			typestring t = make_pointer( create_typedef("_DWORD"));
			cexpr_t * ce = create_helper(t, "test");//make_num(10);
			e->cleanup();//delete 
			e->replace_by(ce);	
		}
	}
}

static void convert_zeroes(cfunc_t *cfunc)
{
  struct zero_converter_t : public ctree_visitor_t
  {
    zero_converter_t(void) : ctree_visitor_t(CV_FAST) {}
    int idaapi visit_expr(cexpr_t *e)
    {
      
      switch ( e->op )
      {

	  case cot_sub:  // B
		  {			  
			  convert_test(e);
          }
          break;

      }
      return 0; // continue walking the tree
    }
  };
  zero_converter_t zc;
  zc.apply_to(&cfunc->body, NULL);
}


static int idaapi callback(void *, hexrays_event_t event, va_list va)
{
	switch ( event )
	{

	case hxe_maturity:
    {
      cfunc_t *cfunc = va_arg(va, cfunc_t *);
      ctree_maturity_t new_maturity = va_argi(va, ctree_maturity_t);
	  if ( new_maturity == CMAT_FINAL ) // ctree is ready
      {
        convert_zeroes(cfunc);
      }
    }
break;

	}
	return 0;
}


