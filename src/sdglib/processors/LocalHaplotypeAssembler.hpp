//
// Created by Bernardo Clavijo (EI) on 18/07/2018.
//

#ifndef BSG_LOCALHAPLOTYPEASSEMBLER_HPP
#define BSG_LOCALHAPLOTYPEASSEMBLER_HPP


#include <sdglib/workspace/WorkSpace.hpp>

class LocalHaplotypeAssembler {
public:
    //Constructor from ws and backbone, creates lists of tags, read ids, anchor sequences and whatnot.
    LocalHaplotypeAssembler(WorkSpace & _ws) :
    ws(_ws),
    assembly(ws),
    uniqueKmerIndex(assembly,31),
    unique63merIndex(assembly)
    {};
    void init_from_backbone(std::vector<sgNodeID_t > _backbone);

    //TODO: Constructor from ws and problem file. Reads everything but the ws/graph from the file;
    void init_from_file( std::string problem_filename);

    //TODO: Constructor from full file. Reads everything including a slimmed down ws/graph from the file;
    void init_from_full_file(std::string problem_filename);

    //TODO: write down to disk (just the problem, will need ws when loading)
    void write_problem(std::string prefix);

    //TODO: write down to disk (full set of things, including a slimmed down ws)
    void write_full(std::string prefix);

    //TODO: and yes, also perform the assembly ;)
    void path_all_reads(); //creates a path for every read, both 10x and LMP
    void path_linked_reads();
    void path_linked_reads_informative_singles();
    void path_paired_reads_informative_singles();
    uint64_t expand_canonical_repeats();
    uint64_t expand_canonical_repeats_direct(int max_rep_size);
    uint64_t pop_short_bubbles(); //uses short paths to pop bubbles
    uint64_t unroll_short_loops();
    void assemble(int k, int min_cov, bool tag_cov, bool simplify=true, std::string output_prefix="");
    void construct_patches();
    //TODO: construct_extensions
    void construct_patched_backbone(bool single_scaffold=true, bool extend_ends=true, bool extend_internals=true);

    void write_gfa(std::string filename);
    void write_anchors(std::string filename);
    void write_patches(std::string filename);
    void write_patched_backbone(std::string filename);
    std::vector<std::pair<std::string, std::string>> compute_metrics();

    void problem_analysis(std::string prefix);


    WorkSpace & ws;
    std::vector<sgNodeID_t > backbone;
    std::vector<Node > backbone_nodes;
    std::set<bsg10xTag> tagSet;
    std::vector<std::pair<uint16_t , std::vector<uint64_t>>> paired_reads;
    std::vector<std::pair<uint16_t , std::vector<uint64_t>>> long_reads;
    std::vector<std::pair<std::pair<sgNodeID_t ,sgNodeID_t>,std::string>> patches;
    std::vector<std::string> patched_backbone;

    SequenceDistanceGraph assembly;
    UniqueKmerIndex uniqueKmerIndex;
    Unique63merIndex unique63merIndex;
    std::vector<std::vector<sgNodeID_t>> linkedread_paths;
    std::vector<std::vector<sgNodeID_t>> pairedread_paths;

    static const sdgVersion_t min_compat_problem = 0x0003;
    static const sdgVersion_t min_compat_full_problem = 0x0003;
};


#endif //BSG_LOCALHAPLOTYPEASSEMBLER_HPP
