//
// Created by Ben Ward (EI) on 26/01/2018.
//

#include "cxxopts.hpp"
#include "sglib/SequenceThreader.h"
#include "sglib/filesystem/check_or_create_directory.h"
#include <sglib/logger/OutputLog.h>

int main(int argc, char **argv) {

    // Input options - user specifies the graph file, the reference FASTA and the output prefix.
    std::string graph_filename;
    std::string reference_filename;
    std::string output_prefix;
    int log_level;

    // DEFAULT PARAMETERS... FIXED FOR NOW.

    // TODO: Make these variable.
    uint8_t m(1), n(1), k(31), min_matches(2);

    std::string outdefault("THREADER_1");

// @formatter:off
    cxxopts::Options options("seq-threader", "Locating query sequences in genome assembly graphs.");

    options.add_options()
            ("help", "Print help")
            ("k", "Kmer size", cxxopts::value<uint8_t>(k)->default_value("31"), "1-31")
            ("r,reference", "Reference FASTA of sequences to locate in graph", cxxopts::value<std::string>(reference_filename), "FASTA - Sequence file")
            ("g,graph", "Genome graph in GFA format", cxxopts::value<std::string>(graph_filename), "GFA file")
            ("o,output", "Output file prefix", cxxopts::value<std::string>(output_prefix) -> default_value(outdefault), "prefix_dir")
            ("l,logging", "Logging level (0: INFO, 1: WARN, 2: DEBUG)", cxxopts::value<int>(log_level) -> default_value("0"), "log_level");
// @formatter:on

    auto result (options.parse(argc, argv));

    if (0 != result.count("help")) {
        std::cout << options.help({""}) << std::endl;
        exit(0);
    }

    sglib::OutputLogLevel = (sglib::LogLevels) log_level;

    auto fail = false;

    if (graph_filename.empty()) {
        std::cout << "Error: The assembly graph file parameter wasn't specified, " << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        fail = true;
    } else if (!std::ifstream(reference_filename)) {
        std::cout << reference_filename << " does not exist." << std::endl;
        fail = true;
    }

    if (!sglib::check_or_create_directory(output_prefix)) {
        std::cout << "Could not find or create output directory: " << output_prefix << '.' << std::endl;
        fail = true;
    }

    if (fail) {
        exit(1);
    }

// LOAD GENOME GRAPH...
    SequenceGraph sg;
    sg.load_from_gfa(graph_filename);

// CONSTRUCT SEQUENCE_MAPPER...
    SequenceThreader tdr(sg, k);
    // Thread FASTA sequences into graph.
    tdr.map_sequences(1, reference_filename, output_prefix);

// CONNECT MAPPINGS...
    tdr.mappings_paths();

// DUMP OUTPUT...

    // Mappings dump.
    std::ofstream mappingdumpout(output_prefix + "mappings_dump.txt");
    tdr.print_mappings(mappingdumpout);
    mappingdumpout.close();

    // Paths dump.
    std::ofstream pathdumpout(output_prefix + "/paths_dump.txt");
    tdr.print_paths(pathdumpout);
    pathdumpout.close();

    // Dark nodes dump.
    std::ofstream darkdump(output_prefix + "dark_nodes.txt");
    tdr.print_dark_nodes(darkdump);
    darkdump.close();

    std::ofstream pathdumpold(output_prefix + "/paths_dump_oldnames.txt");
    tdr.print_paths(pathdumpold, true);
    pathdumpold.close();

    std::ofstream graph_paths_out(output_prefix + "/mapped_paths.fasta");
    tdr.graph_paths_to_fasta(graph_paths_out);
    graph_paths_out.close();
    std::ofstream sizesout(output_prefix + "/paths_sizes.txt");
    tdr.print_unique_paths_sizes(sizesout);
    sizesout.close();

    std::ofstream query_paths_out(output_prefix + "/query_mapped_paths.fasta");
    tdr.query_paths_to_fasta(query_paths_out);
    query_paths_out.close();
}