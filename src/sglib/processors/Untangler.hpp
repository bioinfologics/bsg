//
// Created by Bernardo Clavijo (EI) on 26/02/2018.
//

#ifndef BSG_UNTANGLER_HPP
#define BSG_UNTANGLER_HPP


#include <sglib/workspace/WorkSpace.hpp>
class Untangler;
/**
 * @brief a set of ordered nodes. Can be contiguous or not. node ids refer to nodes in ws.
 */
class Backbone {
public:
    explicit Backbone(WorkSpace & _ws, Untangler & _u): ws(_ws),u(_u) {};


    void sort_and_remove_transitive_links();
    bool is_linear();
    std::string get_sequence();


    WorkSpace &ws;
    Untangler &u;
    std::vector<sgNodeID_t> nodes;
    std::vector<Link> links;
};

class PairedReadLinker {
public:
    PairedReadLinker(WorkSpace & _ws, Untangler & _u): ws(_ws),u(_u) {
        links.resize(ws.getGraph().nodes.size());
    };
    PairedReadLinker(WorkSpace & _ws, Untangler & _u, PairedReadLinker & original, std::vector<sgNodeID_t> selected_nodes): ws(_ws),u(_u) {
        links.resize(ws.getGraph().nodes.size());
        std::set<sgNodeID_t> nodeset;
        for (auto n:selected_nodes) nodeset.insert(llabs(n));
        for (auto n:selected_nodes){
            for (auto l:original.get_fw_links(n)) if (llabs(l.source)<=llabs(l.dest) and nodeset.count(llabs(l.dest))) add_link(l.source,l.dest,l.dist);
            for (auto l:original.get_bw_links(n)) if (llabs(l.source)<=llabs(l.dest) and nodeset.count(llabs(l.dest))) add_link(l.source,l.dest,l.dist);
        }
    };
    void generate_links_size_ci( uint32_t min_size=1000, float min_ci=.75, float max_ci=1.25,int min_reads=5) ;
    void generate_links_hspnp( int min_reads=5) ;
    void generate_links( const std::vector<bool> &to_link,int min_reads=5);
    void add_link( sgNodeID_t source, sgNodeID_t dest, int32_t d);
    void remove_link(sgNodeID_t source, sgNodeID_t dest);
    std::vector<Link> get_fw_links( sgNodeID_t n);
    inline std::vector<Link> get_bw_links( sgNodeID_t n){ return get_fw_links (-n); };
    std::set<sgNodeID_t> fw_reached_nodes(sgNodeID_t n, int radius);
    void remove_transitive_links(int radius);
    void print_perfect_chains();
    std::vector<std::vector<sgNodeID_t>> find_local_problems(uint64_t long_node_size);
    std::vector<std::vector<sgNodeID_t>> solve_local_problem(std::vector<sgNodeID_t> connected_nodes);
    WorkSpace &ws;
    Untangler &u;
    std::vector<std::vector<Link>> links;
};

class Untangler {
    WorkSpace &ws;
public:
    explicit Untangler(WorkSpace & _ws): ws(_ws) {};
    Untangler(Untangler &other) = delete;
    //Functions evaluating 10x tags linkage / coverage
    /**
     * @brief returns the percentage of reads in both ends of node covered by tags in tags
     * @param node
     * @param tags
     * @param end_perc
     * @param end_size
     * @return
     */
    std::pair<float,float> tag_read_percentage_at_ends(sgNodeID_t node, std::set<bsg10xTag> tags, float end_perc=.1, uint32_t end_size=0);
    std::vector<std::vector<std::pair<sgNodeID_t,uint32_t>>> find_tag_neighbours(uint32_t min_size, float min_ci, float max_ci);

    /**
     * @brief grabs all "long" haplotype-specific nodes, uses tags to find neighbours, uses imbalance to impute direction.
     * @param min_size Minimum node size
     * @param min_ci Minimum compression index
     * @param max_ci Maximum compression index
     * @param end_perc Percentage of end covered
     * @return
    */
    std::vector<Link> find_tag_neighbours_with_imbalance(uint32_t min_size, float min_ci, float max_ci, float end_perc=.1);
    std::vector<SequenceGraphPath> get_all_tag_covered_paths(sgNodeID_t from, sgNodeID_t to, std::set<bsg10xTag> &tags, BufferedTagKmerizer &btk);


    //Backbone creation
    /**
     *
     * @param min_size Minimum node size
     * @param min_ci Minimum compression index
     * @param max_ci Maximum compression index
     * @param end_perc Percentage of end covered
     * @param min_shared_tags Minimum number of shared tags
     * @return
     * Returns an unlinked KCI backbone of nodes
     */
    std::vector<Backbone> create_backbones(uint64_t min_size, float min_ci, float max_ci, float end_perc, int min_shared_tags);

    //graph simplification (direct operations, no backbones)
    void unroll_simple_loops();
    void pop_errors_by_ci_and_paths(uint32_t min_size, uint32_t max_size);
    uint64_t expand_canonical_repeats_by_tags(float min_ci, float max_ci, int min_tags=10);
    std::vector<std::pair<sgNodeID_t,sgNodeID_t>> solve_bubbly_paths();
    /**
     * @brief solves a single bubbly path, returns the paths for the 2 new nodes or empty paths if unsolved
     * @param bp Input bubbly path
     * @param no_tags Output parameter defining if the number of tags is higher than min_tags
     * @return
     */
    std::pair<SequenceGraphPath,SequenceGraphPath> solve_bubbly_path(const SequenceSubGraph & bp, bool & no_tags);
    std::vector<std::pair<SequenceGraphPath,SequenceGraphPath>> solve_bubbly_path_2(const SequenceSubGraph & bp);

    uint64_t solve_canonical_repeats_by_tags(std::unordered_set<uint64_t> & reads_to_remap); //TODO: deprecate

    void analise_paths_through_nodes(); //TODO: deprecate

    //graph manipulation TODO: move to SequenceGraph
    std::vector<std::pair<sgNodeID_t, sgNodeID_t>> get_all_HSPNPs();
    std::vector<std::pair<sgNodeID_t,sgNodeID_t>> find_bubbles(uint32_t min_size,uint32_t max_size);
    void dettach_path_as_new_node(sgNodeID_t from, sgNodeID_t to, SequenceGraphPath path);
    std::vector<SequenceGraphPath> make_parallel_paths(std::vector<SequenceGraphPath>);
    bool all_nodes_consumed(std::vector<SequenceGraphPath>);
    std::vector<sgNodeID_t> shared_nodes(std::vector<std::vector<SequenceGraphPath>> parallel_paths);

    //TODO: just deprecate these ASAP

    void connect_neighbours_trivial(uint64_t min_size, float min_ci, float max_ci, int64_t max_distance,
                                     const std::vector<std::vector<std::pair<sgNodeID_t,uint32_t>>> &tagneighbours,
                                     const std::vector<std::vector<std::pair<sgNodeID_t,int64_t>>> & bndist,
                                     const std::vector<std::vector<std::pair<sgNodeID_t,int64_t>>> & fndist);

    uint64_t connect_neighbours_paths_to_same(uint64_t min_size, float min_ci, float max_ci, int64_t max_distance,
                                          const std::vector<std::vector<std::pair<sgNodeID_t,uint32_t>>> &tagneighbours,
                                          const std::vector<std::vector<std::pair<sgNodeID_t,int64_t>>> & bndist,
                                          const std::vector<std::vector<std::pair<sgNodeID_t,int64_t>>> & fndist);

};


#endif //BSG_UNTANGLER_HPP
