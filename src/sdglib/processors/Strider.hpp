//
// Created by Bernardo Clavijo (EI) on 01/05/2020.
//

#pragma once

#include <sdglib/workspace/WorkSpace.hpp>
#include <sdglib/mappers/LongReadsRecruiter.hpp>

class Strider {
public:

    Strider(WorkSpace & _ws);
    void add_datastore(const PairedReadsDatastore & datastore) {paired_datastores.emplace_back(&datastore);}
    void add_datastore(const LongReadsRecruiter & datastore) {long_recruiters.emplace_back(&datastore);}


    //walk out from a node
    SequenceDistanceGraphPath stride_out(sgNodeID_t n);

    SequenceDistanceGraphPath stride_out_in_order(sgNodeID_t n, bool use_pair=true, bool collapse_pair=true, bool verbose=false);

    std::vector<Link> link_out_by_lr(sgNodeID_t n, int d=2000);

    void stride_from_anchors(uint32_t min_size=1,float min_kci=0.5, float max_kci=1.5);

    void link_from_anchors(uint32_t min_size=1,float min_kci=0.5, float max_kci=1.5);

    void route_vs_readpaths_stats();


    WorkSpace & ws;
    std::vector<const PairedReadsDatastore *> paired_datastores;
    std::vector<const LongReadsRecruiter *> long_recruiters;
    std::vector<std::vector<sgNodeID_t>> routes_fw;
    std::vector<std::vector<sgNodeID_t>> routes_bw;
    std::vector<bool> is_anchor;
    std::vector<std::vector<Link>> links_fw;
    std::vector<std::vector<Link>> links_bw;

    static const std::string logo;
    bool experimental_striding=false;
};

