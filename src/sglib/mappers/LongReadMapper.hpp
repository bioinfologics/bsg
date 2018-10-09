//
// Created by Luis Yanes (EI) on 12/02/2018.
//

#ifndef BSG_LONGREADMAPPER_H
#define BSG_LONGREADMAPPER_H


#include <iostream>
#include <sglib/factories/KMerIDXFactory.h>
#include <sglib/datastores/LongReadsDatastore.hpp>
#include <sglib/graph/SequenceGraph.hpp>
#include <sglib/types/MappingTypes.hpp>
#include <sglib/indexers/NKmerIndex.hpp>
#include "LinkedReadMapper.hpp"


enum MappingFilterResult {Success, TooShort, NoMappings, NoReadSets, LowCoverage};
/**
 * Long read mapping to the graph, this class manages storage and computation of the alignments.
 */
class LongReadMapper {

    const SequenceGraph & sg;

    uint8_t k=15;
    int min_size=1000;
    int min_chain=50;
    int max_jump=500;
    int max_delta_change=60;

    /**
     * Stores an index of the mappings of a node to all the mappings where it appears.
     * This index can be queried to get information about all reads that map to a node.
     */
    std::vector< std::vector < std::vector<LongReadMapping>::size_type > > reads_in_node;        /// Reads matching node

    NKmerIndex assembly_kmers;

    void update_indexes_from_mappings();

public:

    LongReadMapper(SequenceGraph &sg, LongReadsDatastore &ds, uint8_t k=15);
    ~LongReadMapper();

    LongReadMapper operator=(const LongReadMapper &other);

    LongReadsDatastore& getLongReadsDatastore() {return datastore;}

    std::vector<uint64_t> get_node_read_ids(sgNodeID_t nodeID) const ;

    void set_params(uint8_t _k=15, int _min_size=1000, int _min_chain=50, int _max_jump=500, int _max_delta_change = 60){
        k=_k;
        min_size=_min_size;
        min_chain=_min_chain;
        max_jump=_max_jump;
        max_delta_change=_max_delta_change;
    }

    void get_all_kmer_matches(std::vector<std::vector<std::pair<int32_t, int32_t>>> & matches, std::vector<std::pair<bool, uint64_t>> & read_kmers);

    std::set<sgNodeID_t> window_candidates(std::vector<std::vector<std::pair<int32_t, int32_t>>> & matches, uint32_t read_kmers_size);

    std::vector<LongReadMapping> alignment_blocks(uint32_t readID, std::vector<std::vector<std::pair<int32_t, int32_t>>> & matches,  uint32_t read_kmers_size, std::set<sgNodeID_t> &candidates);

    std::vector<LongReadMapping> filter_blocks(std::vector<LongReadMapping> & blocks, std::vector<std::vector<std::pair<int32_t, int32_t>>> & matches,  uint32_t read_kmers_size);

    std::vector<LongReadMapping> refine_multinode_reads();

    void map_reads(std::unordered_set<uint32_t> readIDs = {},std::string detailed_log="");

    void map_reads(std::string detailed_log){map_reads({},detailed_log);};

    void read(std::string filename);

    void read(std::ifstream &inf);

    void write(std::string filename);

    void write(std::ofstream &output_file);

    void update_graph_index();

    /**
     * This goes read by read, and filters the mappings by finding a set of linked nodes that maximises 1-cov of the read
     *
     * Unfiltered mappings read from this->mappings and results stored in this->filtered_read_mappings, which is cleared.
     *
     * @param lrm a LinkedReadMapper with mapped reads, over the same graph this mapper has mapped Long Reads.
     * @param min_size minimum size of the read to filter mappings.
     * @param min_tnscore minimum neighbour score on linked reads
     */
    void filter_mappings_with_linked_reads(const LinkedReadMapper &lrm, uint32_t min_size=10000, float min_tnscore=0.03);

    /**
     * Single-read nano10x filtering. Return status
     * @param lrm
     * @param lrbsg
     * @param min_size
     * @param min_tnscore
     * @param readID
     * @param offset_hint
     * @return
     */
    MappingFilterResult filter_mappings_with_linked_reads(const LinkedReadMapper &lrm, BufferedSequenceGetter &lrbsg, uint32_t min_size, float min_tnscore, uint64_t readID, uint64_t offset_hint=0);

    LongReadsDatastore datastore;
    /**
     * This public member stores a flat list of mappings from the reads, it is accessed using the mappings_in_node index
     * or the read_to_mappings index.
     */
    std::vector<LongReadMapping> mappings;
    std::vector < std::vector<LongReadMapping> > filtered_read_mappings;
    /**
     * Stores an index of the resulting mappings of a single long read, for each long read, stores the position of it's mappings.
     * This index can be used to query all the nodes that map to a single read.
     */
    std::vector< std::vector < std::vector<LongReadMapping>::size_type > > read_to_mappings;    /// Nodes in the read, 0 or empty = unmapped


    static const bsgVersion_t min_compat;

};


#endif //BSG_LONGREADMAPPER_H
