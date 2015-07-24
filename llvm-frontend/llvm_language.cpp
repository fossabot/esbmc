/*******************************************************************\

Module: C++ Language Module

Author: Daniel Kroening, kroening@cs.cmu.edu

\*******************************************************************/

#include <llvm_language.h>

#include "llvm_convert.h"

static llvm::cl::OptionCategory esbmc_llvm("esmc_llvm");

llvm_languaget::llvm_languaget(std::vector<std::string> _files)
  : files(_files)
{
}

llvm_languaget::~llvm_languaget()
{
}

bool llvm_languaget::parse()
{
  // From the clang tool example,
  int num_args = 2 + files.size();
  const char **the_args = (const char**) malloc(sizeof(const char*) * num_args);

  unsigned int i=0;
  the_args[i++] = "clang";
  for(; i <= files.size(); ++i)
    the_args[i] = files.at(i-1).c_str();
  the_args[i] = "--";

  clang::tooling::CommonOptionsParser OptionsParser(
    num_args,
    the_args,
    esbmc_llvm);
  free(the_args);

  clang::tooling::ClangTool Tool(
    OptionsParser.getCompilations(),
    OptionsParser.getSourcePathList());

  Tool.buildASTs(ASTs);

  // Use diagnostics to find errors, rather than the return code.
  for (const auto &astunit : ASTs) {
    if (astunit->getDiagnostics().hasErrorOccurred()) {
      std::cerr << std::endl;
      return true;
    }
  }

  return false;
}

bool llvm_languaget::convert(contextt &context)
{
  contextt new_context;
  llvm_convertert converter(context);
  converter.ASTs.swap(ASTs);

  return converter.convert();
}

bool llvm_languaget::preprocess(
  const std::string &path,
  std::ostream &outstream,
  message_handlert &message_handler)
{
  std::cout << "Method " << __PRETTY_FUNCTION__ << " not implemented yet" << std::endl;
  abort();
  return true;
}

void llvm_languaget::internal_additions(std::ostream &out)
{
  std::cout << "Method " << __PRETTY_FUNCTION__ << " not implemented yet" << std::endl;
  abort();
}

bool llvm_languaget::from_expr(
  const exprt &expr,
  std::string &code,
  const namespacet &ns)
{
  std::cout << "Method " << __PRETTY_FUNCTION__ << " not implemented yet" << std::endl;
  abort();
  return true;
}

bool llvm_languaget::from_type(
  const typet &type,
  std::string &code,
  const namespacet &ns)
{
  std::cout << "Method " << __PRETTY_FUNCTION__ << " not implemented yet" << std::endl;
  abort();
  return true;
}

bool llvm_languaget::to_expr(
  const std::string &code,
  const std::string &module __attribute__((unused)),
  exprt &expr,
  message_handlert &message_handler,
  const namespacet &ns)
{
  std::cout << "Method " << __PRETTY_FUNCTION__ << " not implemented yet" << std::endl;
  abort();
  return true;
}
