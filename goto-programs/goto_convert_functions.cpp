/*******************************************************************\

Module: Goto Programs with Functions

Author: Daniel Kroening

Date: June 2003

\*******************************************************************/

#include <assert.h>

#include <base_type.h>
#include <prefix.h>
#include <std_code.h>
#include <std_expr.h>

#include "goto_convert_functions.h"
#include "goto_inline.h"
#include "remove_skip.h"

/*******************************************************************\

Function: goto_convert_functionst::goto_convert_functionst

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

goto_convert_functionst::goto_convert_functionst(
  contextt &_context,
  const optionst &_options,
  goto_functionst &_functions,
  message_handlert &_message_handler):
  goto_convertt(_context, _options, _message_handler),
  functions(_functions)
{
	if (options.get_bool_option("no-inlining"))
	  inlining=false;
	else
	  inlining=true;
}

/*******************************************************************\

Function: goto_convert_functionst::~goto_convert_functionst

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

goto_convert_functionst::~goto_convert_functionst()
{
}

/*******************************************************************\

Function: goto_convert_functionst::goto_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_convert_functionst::goto_convert()
{
  // warning! hash-table iterators are not stable

  typedef std::list<irep_idt> symbol_listt;
  symbol_listt symbol_list;

  forall_symbols(it, context.symbols)
  {
    if(!it->second.is_type && it->second.type.is_code())
      symbol_list.push_back(it->first);
  }

  for(symbol_listt::const_iterator
      it=symbol_list.begin();
      it!=symbol_list.end();
      it++)
  {
    convert_function(*it);
  }

  functions.compute_location_numbers();
}

/*******************************************************************\

Function: goto_convert_functionst::hide

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_convert_functionst::hide(const goto_programt &goto_program)
{
  for(goto_programt::instructionst::const_iterator
      i_it=goto_program.instructions.begin();
      i_it!=goto_program.instructions.end();
      i_it++)
  {
    for(goto_programt::instructiont::labelst::const_iterator
        l_it=i_it->labels.begin();
        l_it!=i_it->labels.end();
        l_it++)
    {
      if(*l_it=="__ESBMC_HIDE")
        return true;
    }
  }

  return false;
}

/*******************************************************************\

Function: goto_convert_functionst::add_return

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_convert_functionst::add_return(
  goto_functiont &f,
  const locationt &location)
{
  if(!f.body.instructions.empty() &&
     f.body.instructions.back().is_return())
    return; // not needed, we have one already

  // see if we have an unconditional goto at the end
  if(!f.body.instructions.empty() &&
     f.body.instructions.back().is_goto() &&
     is_constant_bool2t(f.body.instructions.back().guard) &&
     to_constant_bool2t(f.body.instructions.back().guard).constant_value)
    return;

  goto_programt::targett t=f.body.add_instruction();
  t->make_return();
  t->location=location;

  const typet &thetype = (f.type.return_type().id() == "symbol")
                         ? ns.follow(f.type.return_type())
                         : f.type.return_type();
  exprt rhs=exprt("sideeffect", thetype);
  rhs.statement("nondet");

  expr2tc tmp_expr;
  migrate_expr(rhs, tmp_expr);
  t->code = code_return2tc(tmp_expr);
}

/*******************************************************************\

Function: goto_convert_functionst::convert_function

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_convert_functionst::convert_function(const irep_idt &identifier)
{
  goto_functiont &f=functions.function_map[identifier];
  const symbolt &symbol=ns.lookup(identifier);

  // make tmp variables local to function
  tmp_symbol_prefix=id2string(symbol.name)+"::$tmp::";
  temporary_counter=0;

  f.type=to_code_type(symbol.type);
  f.body_available=symbol.value.is_not_nil();

  if(!f.body_available) return;

  if(!symbol.value.is_code())
  {
    err_location(symbol.value);
    throw "got invalid code for function `"+id2string(identifier)+"'";
  }

  const code_typet::argumentst &arguments=f.type.arguments();

  std::list<irep_idt> arg_ids;

  // add as local variables
  for(code_typet::argumentst::const_iterator
      it=arguments.begin();
      it!=arguments.end();
      it++)
  {
    if(inductive_step)
    {
      // Fix for of arguments
      exprt arg=*it;
      arg.identifier(arg.find("#identifier").id());
      arg.id("symbol");
      arg.remove("#identifier");
      arg.remove("#base_name");
      arg.remove("#location");

      get_struct_components(arg);
    }

    const irep_idt &identifier=it->get_identifier();
    assert(identifier!="");
    arg_ids.push_back(identifier);
  }

  if(!symbol.value.is_code())
  {
    err_location(symbol.value);
    throw "got invalid code for function `"+id2string(identifier)+"'";
  }

  codet tmp(to_code(symbol.value));

  locationt end_location;

  if(to_code(symbol.value).get_statement()=="block")
    end_location=static_cast<const locationt &>(
        symbol.value.end_location());
  else
    end_location.make_nil();

  targets=targetst();
  targets.return_set=true;
  targets.return_value=
      f.type.return_type().id()!="empty" &&
      f.type.return_type().id()!="constructor" &&
      f.type.return_type().id()!="destructor";

  goto_convert_rec(tmp, f.body);

  // add non-det return value, if needed
  if(targets.return_value)
    add_return(f, end_location);

  // add "end of function"
  goto_programt::targett t=f.body.add_instruction();
  t->type=END_FUNCTION;
  t->location=end_location;
  //t->code.identifier(identifier);
  //XXXjmorse, disabled in migration, don't think this does anything

  if(to_code(symbol.value).get_statement()=="block")
    t->location=static_cast<const locationt &>(
        symbol.value.end_location());

  // do local variables
  Forall_goto_program_instructions(i_it, f.body)
  {
    i_it->add_local_variables(arg_ids);
    i_it->function=identifier;
  }

  // remove_skip depends on the target numbers
  f.body.compute_target_numbers();

  remove_skip(f.body);

  f.body.update();

  if(hide(f.body))
    f.type.hide(true);
}

/*******************************************************************\

Function: goto_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_convert(
  codet &code,
  contextt &context,
  const optionst &options,
  goto_functionst &functions,
  message_handlert &message_handler)
{
}

/*******************************************************************\

Function: goto_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_convert(
  contextt &context,
  const optionst &options,
  goto_functionst &functions,
  message_handlert &message_handler)
{
  goto_convert_functionst goto_convert_functions(
    context, options, functions, message_handler);

  try
  {
    goto_convert_functions.thrash_type_symbols();
    goto_convert_functions.goto_convert();
  }

  catch(int)
  {
    goto_convert_functions.error();
  }

  catch(const char *e)
  {
    goto_convert_functions.error(e);
  }

  catch(const std::string &e)
  {
    goto_convert_functions.error(e);
  }

  if(goto_convert_functions.get_error_found())
    throw 0;
}

void
goto_convert_functionst::collect_type(const irept &type, typename_sett &deps)
{

  if (type.id() == "pointer")
    return;

  if (type.id() == "symbol") {
    deps.insert(type.identifier());
    return;
  }

  collect_expr(type, deps);
  return;
}

void
goto_convert_functionst::collect_expr(const irept &expr, typename_sett &deps)
{

  if (expr.id() == "pointer")
    return;

  forall_irep(it, expr.get_sub()) {
    collect_expr(*it, deps);
  }

  forall_named_irep(it, expr.get_named_sub()) {
    if (it->first == "type" || it->first == "subtype")
      collect_type(it->second, deps);
    else
      collect_type(it->second, deps);
  }

  forall_named_irep(it, expr.get_comments()) {
    collect_type(it->second, deps);
  }

  return;
}

void
goto_convert_functionst::rename_types(irept &type, const symbolt &cur_name_sym,
                                      const irep_idt &sname)
{

  if (type.id() == "pointer")
    return;

  // In a vastly irritating turn of events, some type symbols aren't entirely
  // correct. This is because (in the current 27_exStbFb test) some type
  // symbols get the module name inserted into the -- so c::int32_t becomes
  // c::main::int32_t.
  // Now this makes entire sense, because int32_t could be something else in
  // some other file. However, because type symbols aren't squashed at type
  // checking time (which, you know, might make sense) we now don't know
  // what type symbol to link "c::int32_t" up to. Can't push it back to
  // type checking either as I don't understand it, and it means touching
  // the C++ frontend.
  // So; instead we test to see whether a type symbol is linked correctly, and
  // if it isn't we look up what module the current block of code came from and
  // try to guess what type symbol it should have.

  typet type2;
  if (type.id() == "symbol") {
    if (type.identifier() == sname) {
      // A recursive symbol -- the symbol we're about to link to is in fact the
      // one that initiated this chain of renames. This leads to either infinite
      // loops or segfaults, depending on the phase of the moon.
      // It should also never happen, but with C++ code it does, because methods
      // are part of the type, and methods can take a full struct/object as a
      // parameter, not just a reference/pointer. So, that's a legitimate place
      // where we have this recursive symbol dependancy situation.
      // The workaround to this is to just ignore it, and hope that it doesn't
      // become a problem in the future.
      return;
    }

    const symbolt *sym;
    if (!ns.lookup(type.identifier(), sym)) {
      // If we can just look up the current type symbol, use that.
      type2 = ns.follow((typet&)type);
    } else {
      // Otherwise, try to guess the namespaced type symbol
      std::string ident = type.identifier().as_string();
      std::string ident2;

      // Detect module prefix, then insert module name after it.
      if (ident.c_str()[0] == 'c' && ident.c_str()[1] == 'p' &&
          ident.c_str()[2] == 'p') {
        ident2 = "cpp::" + cur_name_sym.module.as_string() + "::" +
                 ident.substr(5, std::string::npos);
      } else {
        ident2 = "c::" + cur_name_sym.module.as_string() + "::"  +
                 ident.substr(3, std::string::npos);
      }

      // Try looking that up.
      if (!ns.lookup(irep_idt(ident2), sym)) {
        irept tmptype = type;
        tmptype.identifier(irep_idt(ident2));
        type2 = ns.follow((typet&)tmptype);
      } else {
        // And if we fail
        std::cerr << "Can't resolve type symbol " << ident;
        std::cerr << " at symbol squashing time" << std::endl;
        abort();
      }
    }

    type = type2;
    return;
  }

  rename_exprs(type, cur_name_sym, sname);
  return;
}

void
goto_convert_functionst::rename_exprs(irept &expr, const symbolt &cur_name_sym,
                                      const irep_idt &sname)
{

  if (expr.id() == "pointer")
    return;

  Forall_irep(it, expr.get_sub())
    rename_exprs(*it, cur_name_sym, sname);

  Forall_named_irep(it, expr.get_named_sub()) {
    if (it->first == "type" || it->first == "subtype") {
      rename_types(it->second, cur_name_sym, sname);
    } else {
      rename_exprs(it->second, cur_name_sym, sname);
    }
  }

  Forall_named_irep(it, expr.get_comments())
    rename_exprs(it->second, cur_name_sym, sname);

  return;
}

void
goto_convert_functionst::wallop_type(irep_idt name,
                         std::map<irep_idt, std::set<irep_idt> > &typenames,
                         const irep_idt &sname)
{

  // If this type doesn't depend on anything, no need to rename anything.
  std::set<irep_idt> &deps = typenames.find(name)->second;
  if (deps.size() == 0)
    return;

  // Iterate over our dependancies ensuring they're resolved.
  for (std::set<irep_idt>::iterator it = deps.begin(); it != deps.end(); it++)
    wallop_type(*it, typenames, sname);

  // And finally perform renaming.
  symbolst::iterator it = context.symbols.find(name);
  rename_types(it->second.type, it->second, sname);
  deps.clear();
  return;
}

void
goto_convert_functionst::thrash_type_symbols(void)
{
  forall_symbols(it, context.symbols) {
    if(it->second.static_lifetime && !it->second.type.is_pointer())
      get_struct_components(symbol_expr(it->second));
  }

  // This function has one purpose: remove as many type symbols as possible.
  // This is easy enough by just following each type symbol that occurs and
  // replacing it with the value of the type name. However, if we have a pointer
  // in a struct to itself, this breaks down. Therefore, don't rename types of
  // pointers; they have a type already; they're pointers.

  // Collect a list of all type names. This it required before this entire
  // thing has no types, and there's no way (in C++ converted code at least)
  // to decide what name is a type or not.
  typename_sett names;
  forall_symbols(it, context.symbols) {
    collect_expr(it->second.value, names);
    collect_expr(it->second.type, names);
  }

  // Try to compute their dependencies.

  typename_mapt typenames;

  forall_symbols(it, context.symbols) {
    if (names.find(it->second.name) != names.end()) {
      typename_sett list;
      collect_expr(it->second.value, list);
      collect_expr(it->second.type, list);
      typenames[it->second.name] = list;
    }
  }

  for (typename_mapt::iterator it = typenames.begin(); it != typenames.end(); it++)
    it->second.erase(it->first);

  // Now, repeatedly rename all types. When we encounter a type that contains
  // unresolved symbols, resolve it first, then include it into this type.
  // This means that we recurse to whatever depth of nested types the user
  // has. With at least a meg of stack, I doubt that's really a problem.
  std::map<irep_idt, std::set<irep_idt> >::iterator it;
  for (it = typenames.begin(); it != typenames.end(); it++)
    wallop_type(it->first, typenames, it->first);

  // And now all the types have a fixed form, assault all existing code.
  Forall_symbols(it, context.symbols) {
    rename_types(it->second.type, it->second, it->first);
    rename_exprs(it->second.value, it->second, it->first);
  }

  return;
}
