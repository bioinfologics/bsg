//
// Created by Bernardo Clavijo (EI) on 12/05/2018.
//

#ifndef BSG_PAIREDREADMAPPER_HPP
#define BSG_PAIREDREADMAPPER_HPP

#include <map>
#include <fstream>

#include "sglib/types/MappingTypes.hpp"
#include "sglib/factories/KMerIDXFactory.hpp"
#include "sglib/readers/SequenceGraphReader.hpp"
#include "sglib/SMR.hpp"
#include <sglib/datastores/PairedReadsDatastore.hpp>
#include <sglib/indexers/UniqueKmerIndex.hpp>

class UniqueKmerIndex;
class Unique63merIndex;
class PairedReadConnectivityDetail; //Forward declaration

/**
 * @brief A mapper for linked reads from a PairedReadsDatastore.
 *
 * Supports partial remapping of unmapped reads or of a selection list.
 */

class PairedReadMapper {

public:
    PairedReadMapper(SequenceGraph &_sg, PairedReadsDatastore &_datastore, const UniqueKmerIndex &uki,const Unique63merIndex &u63i) :
    sg(_sg),
    datastore(_datastore),
    kmer_to_graphposition(uki),
    k63mer_to_graphposition(u63i)
    {
        reads_in_node.resize(sg.nodes.size());
    };
    void write(std::ofstream & output_file);
    void read(std::ifstream & input_file);
    void map_reads(std::unordered_set<uint64_t> const &  reads_to_remap={});
    void remap_all_reads();
    void map_reads63(std::unordered_set<uint64_t> const &  reads_to_remap={});
    void remap_all_reads63();

    void remove_obsolete_mappings();
    std::vector<uint64_t> size_distribution();
    void populate_orientation();
    void print_stats();

    PairedReadMapper operator=(const PairedReadMapper &other);

    std::vector<uint64_t> get_node_readpairs_ids(sgNodeID_t);

    const SequenceGraph & sg;
    const UniqueKmerIndex& kmer_to_graphposition;
    const Unique63merIndex& k63mer_to_graphposition;
    const PairedReadsDatastore & datastore;
    std::vector<std::vector<ReadMapping>> reads_in_node;
    std::vector<sgNodeID_t> read_to_node;//id of the main node if mapped, set to 0 to remap on next process
    //TODO: reading and writing this would simplify things??
    std::vector<bool> read_direction_in_node;//0-> fw, 1->rev;
    std::vector<uint64_t> rfdist;
    std::vector<uint64_t> frdist;

    static const bsgVersion_t min_compat;

};

/**
 * @brief Analysis of all reads connecting two particular nodes.
 */

class PairedReadConnectivityDetail {
public:
    PairedReadConnectivityDetail(){};
    PairedReadConnectivityDetail(const PairedReadMapper & prm, sgNodeID_t source, sgNodeID_t dest);
    PairedReadConnectivityDetail& operator+=(const PairedReadConnectivityDetail& rhs){
        this->orientation_paircount[0] += rhs.orientation_paircount[0];
        this->orientation_paircount[1] += rhs.orientation_paircount[1];
        this->orientation_paircount[2] += rhs.orientation_paircount[2];
        this->orientation_paircount[3] += rhs.orientation_paircount[3];
        return *this;
    }

    uint64_t orientation_paircount[4]={0,0,0,0};
};

#endif //BSG_PAIREDREADMAPPER_HPP
