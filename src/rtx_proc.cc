#include <rtx_config.h>
#include <rtx_processor.h>
#include <lttoolbox/cli.h>
#include <lttoolbox/file_utils.h>
#include <lttoolbox/lt_locale.h>
#include <iostream>

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();
  CLI cli("perform structural transfer", PACKAGE_VERSION);
  cli.add_bool_arg('a', "anaphora", "expect coreference LUs from apertium-anaphora");
  cli.add_bool_arg('b', "both", "print text (use with -T)");
  cli.add_bool_arg('e', "everything", "print a complete trace of execution");
  cli.add_bool_arg('f', "filter-trace", "trace filterParseGraph()");
  cli.add_bool_arg('F', "filter", "filter branches more often");
  cli.add_str_arg('m', "mode", "set the mode of tree output, options are 'flat', 'nest', 'latex', 'dot', 'box'", "MODE");
  cli.add_bool_arg('r', "rules", "print the rules that are being applied");
  cli.add_bool_arg('s', "steps", "print the instructions executed by the stack machine");
  cli.add_bool_arg('t', "trx", "mimic the behavior of apertium-transfer and apertium-interchunk");
  cli.add_bool_arg('T', "tree", "print parse trees rather than apply output rules");
  cli.add_bool_arg('z', "null-flush", "flush output on \\0");
  cli.add_bool_arg('h', "help", "print this message and exit");
  cli.add_file_arg("bytecode_file", false);
  cli.add_file_arg("input_file", true);
  cli.add_file_arg("output_file", true);
  cli.parse_args(argc, argv);
  
  RTXProcessor p;
  p.withoutCoref(!cli.get_bools()["anaphora"]);
  p.completeTrace(cli.get_bools()["everything"]);
  p.printFilter(cli.get_bools()["filter-trace"]);
  p.noFiltering(!cli.get_bools()["filter"]);
  p.printRules(cli.get_bools()["rules"]);
  p.printSteps(cli.get_bools()["steps"]);
  p.mimicChunker(cli.get_bools()["trx"]);
  p.setNullFlush(cli.get_bools()["null-flush"]);
  
  bool haveB = cli.get_bools()["both"];
  bool haveT = cli.get_bools()["tree"];
  p.printTrees(haveT);
  p.printText(haveB || (!haveT && !haveB));

  auto args = cli.get_strs();
  if (args.find("mode") != args.end()) {
    auto m = args["mode"][0];
    if (!p.setOutputMode(m)) {
      cout << "\"" << m << "\" is not a recognized tree mode. Valid options are \"flat\", \"nest\", \"latex\", \"dot\", and \"box\"." << endl;
      exit(EXIT_FAILURE);
    }
  }

  p.read(cli.get_files()[0]);
  FILE* input = openInBinFile(cli.get_files()[1]);
  UFILE* output = openOutTextFile(cli.get_files()[2]);

  p.process(input, output);

  fclose(input);
  u_fclose(output);
  return EXIT_SUCCESS;
}
