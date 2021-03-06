//
// Created by Bernardo Clavijo (EI) on 11/02/2018.
//

#pragma once

#include <set>
#include <unordered_set>
#include <map>
#include <sdglib/types/MappingTypes.hpp>
#include <sdglib/indexers/UniqueKmerIndex.hpp>
#include <sdglib/Version.hpp>

class WorkSpace;
class UniqueKmerIndex;
class Unique63merIndex;
class LinkedReadsDatastore;

using LinkedTag = uint32_t;

class TagNeighbour {
public:
    TagNeighbour(){};
    TagNeighbour(sgNodeID_t n, float s):node(n),score(s){};
    sgNodeID_t node;
    float score; //breaking the latte principle
};

/**
 * @brief A mapper for linked reads from a LinkedReadsDatastore.
 *
 * Supports partial remapping of unmapped reads or of a selection list.
 */
class LinkedReadsMapper {

public:
    LinkedReadsMapper(WorkSpace &_ws, LinkedReadsDatastore &_datastore);

    /**
     * @brief Provides an overview of the information in the LinkedReadsMapper
     * @param level Base indentation level to use on the result
     * @param recursive Whether it should explore or not the rest of the hierarchy
     * @return
     * A text summary of the information contained in a LinkedReadsMapper
     */
    std::string ls(int level=0,bool recursive=true) const;

    friend std::ostream& operator<<(std::ostream &os, const LinkedReadsMapper &lirm);

    void write(std::ofstream & output_file);
    void read(std::ifstream & input_file);
    void dump_readpaths(std::string filename);
    void load_readpaths(std::string filename);

    /** @brief Maps the provided LinkedReadsDatastore to the provided graph, saves the results to the reads_in_node and nodes_in_read collections.
     * The mapper only considers unique perfect matches, if a single kmer from the read matches 2 different nodes the read is marked as multi-mapping and the mapping is discarded.
     * If a reads_to_remap set is passed to the function only the selected set of reads is mapped, the rest of the mappings remains as they are.
     *
     * To access the results see the reads_in_node and nodes_in_read collections in this class
     *
     * @param reads_to_remap set of readsIDs to remap
     */
    void map_reads(std::unordered_set<uint64_t> const &  reads_to_remap={});

    /** @brief Clears the reads_in_node and nodes_in_read collections and runs the map_reads() function with no selected set (maps all reads again)
     */
    void remap_all_reads();

    /**
     * Checks that the graph and datastore are the same and if they are assigns reads_in_node and nodes_in_read collections
     *
     * @param other LinkedReadsMapper to compare with
     * @return
     */
    LinkedReadsMapper& operator=(const LinkedReadsMapper &other);

    /**
     * This is the same as map_reads() but using k63
     * @param reads_to_remap Optional argument of set of reads to map.
     */
    void map_reads63(std::unordered_set<uint64_t> const &  reads_to_remap={});

    /**
     * Clears the reads_in_node and nodes_in_read collections and remaps reads to regenerate them.
     */
    void remap_all_reads63();

    /** @brief creates a read path for each read through mapping
     *
     * @return
     */
    void path_reads(uint8_t k=63,int filter=200);

    /** @brief Prints the count of pairs mapped.
     *
     * Prints the count of pairs mapped where no end mapped, a single end mapped and both ends mapped and of those how many mapped to a single node.
     */
    void print_status() const;

    /** @brief Given a nodeID returns a set of all tags mapped to that node.
     * If there are no mapped tags returns an empty set
     *
     * @param n nodeID
     * @return set of bsg10xtags
     */
    std::set<LinkedTag> get_node_tags(sgNodeID_t n);

    /**
     * Creates a nieghbours matrix with all nodes where for each node the function finds all nodes that have tags that
     * cover min_score of the total tags of each node.
     *
     * Example:
     *      - node A has reads with tags 5, 5, 6, 7, 8
     *      - node B has reads with tags 5, 8, 9, 9, 10
     *
     * Then B tags cover 3/6=0.5 of A reads, and A tags cover 2/5=0.4 of B reads.
     * If min_score=.5 then B is in A's neighbours, but A is not in B's
     *
     * Results are stored in the tag_neighbours vector
     * @param min_size
     * @param min_score
     * @param min_mapped_reads_per_tag
     */
    void compute_all_tag_neighbours(int min_size,float min_score, int min_mapped_reads_per_tag=2);

    void write_tag_neighbours(std::string filename);
    void read_tag_neighbours(std::string filename);

    /** @brief Populates the read_direction_in_node collection for the dataset
    *
    */
    void populate_orientation();

    WorkSpace &ws;

    LinkedReadsDatastore &datastore;

    /**
     * Collection of read mappings
     * reads_in_node[nodeID] = [vector of mappings to nodeID... ]
     */
    std::vector<std::vector<ReadMapping>> reads_in_node;

    /**
     * Read to node index
     * read_to_node[readID] = nodeID where the read is mapped to
     */
    std::vector<sgNodeID_t> read_to_node;//id of the main node if mapped, set to 0 to remap on next process

    /**
     * read_direction_in_node[i] has the direction with wich read i was mapped in the corresponding mapping (in read_to_node[i])
     */
    std::vector<bool> read_direction_in_node;//0-> fw, 1->rev;

    /**
     *  Collection of neighbouring nodes
     *  Completed using compute_all_tag_neighbours()
     *  tag_neighbours[nodeID] = [collection of 10 determined neighbours (see TagNeighbour) ...]
     */
    std::vector<std::vector<TagNeighbour>> tag_neighbours; //not persisted yet!
    std::vector<ReadPath> read_paths;

    std::vector<std::vector<int64_t>> paths_in_node;

    static const sdgVersion_t min_compat;
};
