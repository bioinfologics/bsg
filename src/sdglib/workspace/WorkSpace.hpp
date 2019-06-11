//
// Created by Bernardo Clavijo (EI) on 24/02/2018.
//

#ifndef BSG_WORKSPACE_HPP
#define BSG_WORKSPACE_HPP


#include <sdglib/datastores/LinkedReadsDatastore.hpp>
#include <sdglib/mappers/LinkedReadMapper.hpp>
#include <sdglib/datastores/PairedReadsDatastore.hpp>
#include <sdglib/mappers/PairedReadMapper.hpp>
#include <sdglib/datastores/LongReadsDatastore.hpp>
#include <sdglib/mappers/LongReadMapper.hpp>
#include "sdglib/graph/SequenceDistanceGraph.hpp"
#include "sdglib/processors/KmerCompressionIndex.hpp"
#include <sdglib/indexers/UniqueKmerIndex.hpp>

class LogEntry{
public:
    LogEntry(std::time_t t, std::string v, std::string tx):timestamp(t),bsg_version(std::move(v)),log_text(std::move(tx)){};
    std::time_t timestamp;
    std::string bsg_version;
    std::string log_text;
};

class WorkSpace {

public:
    WorkSpace() :
    kci(sdg),
    uniqueKmerIndex(sdg, 31),
    unique63merIndex(sdg){};
    WorkSpace(const WorkSpace& that) = delete; //we definitely do not want copy constructors here, thank you
    void print_log();

    void add_log_entry(std::string text);

    void dump_to_disk(std::string filename);

    void load_from_disk(std::string filename,bool log_only=false);

    //general operations

    void create_index(bool verbose = true) { uniqueKmerIndex.generate_index(sdg,verbose); }
    void create_63mer_index(bool verbose = true) { unique63merIndex.generate_index(sdg,verbose); }
    void remap_all();
    void remap_all63();
    //Projected operations with info from the graph

    std::vector<sgNodeID_t>
    select_from_all_nodes(uint32_t min_size, uint32_t max_size, uint32_t min_tags, uint32_t max_tags, float min_ci, float max_ci);

    KmerCompressionIndex& getKCI() {return kci;}
    SequenceDistanceGraph& getGraph() {return sdg;}
    std::vector<LogEntry> log;

    //All status classes are public, treat them with care anyway ;)
    SequenceDistanceGraph sdg;
    UniqueKmerIndex uniqueKmerIndex;
    Unique63merIndex unique63merIndex;
    std::vector<PairedReadsDatastore> paired_read_datastores;
    std::vector<PairedReadMapper> paired_read_mappers;
    std::vector<LinkedReadsDatastore> linked_read_datastores;
    std::vector<LinkedReadMapper> linked_read_mappers;
    std::vector<LongReadsDatastore> long_read_datastores;
    std::vector<LongReadMapper> long_read_mappers;


    KmerCompressionIndex kci;
    std::string verbose_log="";

    static const bsgVersion_t min_compat;
    std::vector<std::string> read_counts_header;
};


#endif //BSG_WORKSPACE_HPP
