#include <rtx_config.h>
#include <rtx_compiler.h>
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>
#include <lttoolbox/lt_locale.h>
#include <cstdlib>
#include <iostream>
#include <libxml/xmlreader.h>
#include <trx_compiler.h>

using namespace std;

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();
  CLI cli("compile .rtx files", PACKAGE_VERSION);
  cli.add_str_arg('e', "exclude", "exclude a rule by name", "NAME");
  cli.add_str_arg('l', "lexical", "load a file of lexicalized weights", "FILE");
  cli.add_bool_arg('s', "summarize", "print rules to stderr as 'output -> pattern'");
  cli.add_bool_arg('S', "stats", "print statistics about rule file to stdout");
  cli.add_bool_arg('h', "help", "print this message and exit");
  cli.add_file_arg("rule_file", false);
  cli.add_file_arg("bytecode_file", false);
  cli.parse_args(argc, argv);

  bool stats = cli.get_bools()["stats"];
  bool summary = cli.get_bools()["summarize"];
  vector<UString> exclude;
  vector<string> lexFiles;

  auto args = cli.get_strs();
  if (args.find("exclude") != args.end()) {
    for (auto& e : args["exclude"]) {
      exclude.push_back(to_ustring(e.c_str()));
    }
  }
  if (args.find("lexical") != args.end()) {
    for (auto& l : args["lexical"]) {
      lexFiles.push_back(l);
    }
  }

  std::string rules = cli.get_files()[0];
  std::string bin = cli.get_files()[1];

  FILE* check = openInBinFile(rules);
  int c;
  while((c = fgetc(check)) != '<') {
    if(c == EOF) break;
    else if(isspace(c)) continue;
    else break;
  }
  bool xml = (c == '<');
  fclose(check);

  if(xml) {
    TRXCompiler comp;
    if(summary) {
      cout << "Summary mode not available for XML." << endl;
    }
    for(auto lex : lexFiles) comp.loadLex(lex);
    for(auto exc : exclude) comp.excludeRule(exc);
    comp.compile(rules);
    comp.write(bin.c_str());
    if(stats) comp.printStats();
  }
  else {
    RTXCompiler comp;
    comp.setSummarizing(summary);
    for(auto lex : lexFiles) comp.loadLex(lex);
    for(auto exc : exclude) comp.excludeRule(exc);
    comp.read(rules);
    comp.write(bin.c_str());
    if(stats) comp.printStats();
  }

  return EXIT_SUCCESS;
}
