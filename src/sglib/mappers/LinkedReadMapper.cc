//
// Created by Bernardo Clavijo (EI) on 03/11/2017.
//
#include <iostream>
#include <iomanip>
#include <cassert>
#include <atomic>


#include "LinkedReadMapper.hpp"
#include "sglib/SMR.hpp"
#include "sglib/factories/KMerIDXFactory.hpp"
#include "sglib/readers/SequenceGraphReader.hpp"
#include <sglib/utilities/omp_safe.hpp>

const bsgVersion_t LinkedReadMapper::min_compat = 0x0001;

void LinkedReadMapper::write(std::ofstream &output_file) {
    //read-to-node
    uint64_t count=read_to_node.size();
    output_file.write((char *) &BSG_MAGIC, sizeof(BSG_MAGIC));
    output_file.write((char *) &BSG_VN, sizeof(BSG_VN));
    BSG_FILETYPE type(LinkedMap_FT);
    output_file.write((char *) &type, sizeof(type));

    output_file.write((const char *) &count,sizeof(count));
    output_file.write((const char *) read_to_node.data(),sizeof(sgNodeID_t)*count);
    //mappings
    count=reads_in_node.size();
    output_file.write((const char *) &count,sizeof(count));
    for (auto i=0;i<count;++i) {
        uint64_t mcount=reads_in_node[i].size();
        output_file.write((const char *) &mcount,sizeof(mcount));
        output_file.write((const char *) reads_in_node[i].data(), sizeof(ReadMapping) * mcount);
    }
}

void LinkedReadMapper::read(std::ifstream &input_file) {
    uint64_t count;

    bsgMagic_t magic;
    bsgVersion_t version;
    BSG_FILETYPE type;
    input_file.read((char *) &magic, sizeof(magic));
    input_file.read((char *) &version, sizeof(version));
    input_file.read((char *) &type, sizeof(type));

    if (magic != BSG_MAGIC) {
        throw std::runtime_error("Magic number not present in the LinkedReadMap file");
    }

    if (version < min_compat) {
        throw std::runtime_error("LinkedReadMap file version: " + std::to_string(version) + " is not compatible with " + std::to_string(min_compat));
    }

    if (type != LinkedMap_FT) {
        throw std::runtime_error("File type supplied: " + std::to_string(type) + " is not compatible with LinkedMap_FT");
    }


    input_file.read(( char *) &count,sizeof(count));
    read_to_node.resize(count);
    input_file.read(( char *) read_to_node.data(),sizeof(sgNodeID_t)*count);
    input_file.read(( char *) &count,sizeof(count));
    reads_in_node.resize(count);
    for (auto i=0;i<count;++i) {
        uint64_t mcount;
        input_file.read(( char *) &mcount,sizeof(mcount));
        reads_in_node[i].resize(mcount);
        input_file.read(( char *) reads_in_node[i].data(), sizeof(ReadMapping) * mcount);
    }
}

void LinkedReadMapper::remap_all_reads() {
    for (auto &rtn:read_to_node) rtn=0;
    for (auto &rin:reads_in_node) rin.clear();
    map_reads();
}

void LinkedReadMapper::map_reads(const std::unordered_set<uint64_t> &reads_to_remap) {
    const int k = 31;
    reads_in_node.resize(sg.nodes.size());
    read_to_node.resize(datastore.size()+1);
    if (not reads_to_remap.empty())
        sglib::OutputLog()<<reads_to_remap.size()<<" selected reads / "<<read_to_node.size()-1<<" total"<<std::endl;

    /*
     * Read mapping in parallel,
     */
    uint64_t thread_mapped_count[omp_get_max_threads()],thread_total_count[omp_get_max_threads()],thread_multimap_count[omp_get_max_threads()];
    std::vector<ReadMapping> thread_mapping_results[omp_get_max_threads()];
    sglib::OutputLog(sglib::LogLevels::DEBUG)<<"Private mapping initialised for "<<omp_get_max_threads()<<" threads"<<std::endl;
#pragma omp parallel
    {
        const int min_matches=1;
        std::vector<KmerIDX> readkmers;
        StreamKmerIDXFactory skf(31);
        ReadMapping mapping;
        auto blrs=BufferedLRSequenceGetter(datastore,128*1024,260);
        auto & private_results=thread_mapping_results[omp_get_thread_num()];
        auto & mapped_count=thread_mapped_count[omp_get_thread_num()];
        auto & total_count=thread_total_count[omp_get_thread_num()];
        auto & multimap_count=thread_multimap_count[omp_get_thread_num()];
        mapped_count=0;
        total_count=0;
        multimap_count=0;
        bool c ;
        //std::cout<<omp_get_thread_num()<<std::endl;
#pragma omp for
        for (uint64_t readID=1;readID<read_to_node.size();++readID) {
            mapping.read_id = readID;
            //this enables partial read re-mapping by setting read_to_node to 0
            if ((reads_to_remap.size()>0 and reads_to_remap.count(mapping.read_id)>0) or (reads_to_remap.empty() and 0==read_to_node[mapping.read_id])) {
                mapping.node = 0;
                mapping.unique_matches = 0;
                mapping.first_pos = 0;
                mapping.last_pos = 0;
                mapping.rev = false;
                mapping.unique_matches = 0;
                //get all kmers from read
                auto seq=blrs.get_read_sequence(readID);
                readkmers.clear();
                skf.produce_all_kmers(seq,readkmers);

                for (auto &rk:readkmers) {
                    auto nk = kmer_to_graphposition.find(rk.kmer);
                    if (kmer_to_graphposition.end()!=nk) {
                        //get the node just as node
                        sgNodeID_t nknode = llabs(nk->second.node);
                        //TODO: sort out the sign/orientation representation
                        if (mapping.node == 0) {
                            mapping.node = nknode;
                            if ((nk->second.node > 0 and rk.contigID > 0) or
                                (nk->second.node < 0 and rk.contigID < 0))
                                mapping.rev = false;
                            else mapping.rev = true;
                            mapping.first_pos = nk->second.pos;
                            mapping.last_pos = nk->second.pos;
                            ++mapping.unique_matches;
                        } else {
                            if (mapping.node != nknode) {
                                mapping.node = 0;
                                ++multimap_count;
                                break; //exit -> multi-mapping read! TODO: allow mapping to consecutive nodes
                            } else {
                                mapping.last_pos = nk->second.pos;
                                ++mapping.unique_matches;
                            }
                        }
                    }
                }
                if (mapping.node != 0 and mapping.unique_matches >= min_matches) {
                    //optimisation: just save the mapping in a thread private collection for now, have a single thread putting from that into de structure at the end
                    private_results.push_back(mapping);
                    ++mapped_count;
                }
            }
            auto tc = ++total_count;
            if (tc % 10000000 == 0) sglib::OutputLog()<< mapped_count << " / " << tc <<" ("<<multimap_count<<" multi-mapped)"<< std::endl;
        }
    }
    for (auto & tres:thread_mapping_results){
        //sglib::OutputLog(sglib::LogLevels::DEBUG)<<"mixing in "<<tres.size()<<" thread specific results"<<std::endl;
        for (auto &rm:tres){
            read_to_node[rm.read_id] = rm.node;
            reads_in_node[rm.node].emplace_back(rm);
        }
        tres.clear();
        tres.shrink_to_fit();
    }
    uint64_t mapped_count=0,total_count=0,multimap_count=0;
    for (auto i=0;i<omp_get_max_threads();++i){
        mapped_count+=thread_mapped_count[i];
        total_count+=thread_total_count[i];
        multimap_count+=thread_multimap_count[i];
    }
    sglib::OutputLog(sglib::LogLevels::INFO)<<"Reads mapped: "<<mapped_count<<" / "<<total_count<<" ("<<multimap_count<<" multi-mapped)"<<std::endl;
#pragma omp parallel for
    for (sgNodeID_t n=1;n<reads_in_node.size();++n){
        std::sort(reads_in_node[n].begin(),reads_in_node[n].end());
    }
}

void LinkedReadMapper::remap_all_reads63() {
    for (auto &rtn:read_to_node) rtn=0;
    for (auto &rin:reads_in_node) rin.clear();
    map_reads63();
}

void LinkedReadMapper::map_reads63(const std::unordered_set<uint64_t> &reads_to_remap) {
    const int k = 63;
    reads_in_node.resize(sg.nodes.size());
    read_to_node.resize(datastore.size()+1);
    if (not reads_to_remap.empty())
        sglib::OutputLog()<<reads_to_remap.size()<<" selected reads / "<<read_to_node.size()-1<<" total"<<std::endl;

    /*
     * Read mapping in parallel,
     */
    uint64_t thread_mapped_count[omp_get_max_threads()],thread_total_count[omp_get_max_threads()],thread_multimap_count[omp_get_max_threads()];
    std::vector<ReadMapping> thread_mapping_results[omp_get_max_threads()];
    sglib::OutputLog(sglib::LogLevels::DEBUG)<<"Private mapping initialised for "<<omp_get_max_threads()<<" threads"<<std::endl;
#pragma omp parallel
    {
        const int min_matches=1;
        std::vector<KmerIDX128> readkmers;
        StreamKmerIDXFactory128 skf(63);
        ReadMapping mapping;
        auto blrs=BufferedLRSequenceGetter(datastore,128*1024,260);
        auto & private_results=thread_mapping_results[omp_get_thread_num()];
        auto & mapped_count=thread_mapped_count[omp_get_thread_num()];
        auto & total_count=thread_total_count[omp_get_thread_num()];
        auto & multimap_count=thread_multimap_count[omp_get_thread_num()];
        mapped_count=0;
        total_count=0;
        multimap_count=0;
        bool c ;
        //std::cout<<omp_get_thread_num()<<std::endl;
#pragma omp for
        for (uint64_t readID=1;readID<read_to_node.size();++readID) {
            mapping.read_id = readID;
            //this enables partial read re-mapping by setting read_to_node to 0
            if ((reads_to_remap.size()>0 and reads_to_remap.count(mapping.read_id)>0) or (reads_to_remap.empty() and 0==read_to_node[mapping.read_id])) {
                mapping.node = 0;
                mapping.unique_matches = 0;
                mapping.first_pos = 0;
                mapping.last_pos = 0;
                mapping.rev = false;
                mapping.unique_matches = 0;
                //get all kmers from read
                auto seq=blrs.get_read_sequence(readID);
                readkmers.clear();
                skf.produce_all_kmers(seq,readkmers);

                for (auto &rk:readkmers) {
                    auto nk = k63mer_to_graphposition.find(rk.kmer);
                    if (k63mer_to_graphposition.end()!=nk) {
                        //get the node just as node
                        sgNodeID_t nknode = llabs(nk->second.node);
                        //TODO: sort out the sign/orientation representation
                        if (mapping.node == 0) {
                            mapping.node = nknode;
                            if ((nk->second.node > 0 and rk.contigID > 0) or
                                (nk->second.node < 0 and rk.contigID < 0))
                                mapping.rev = false;
                            else mapping.rev = true;
                            mapping.first_pos = nk->second.pos;
                            mapping.last_pos = nk->second.pos;
                            ++mapping.unique_matches;
                        } else {
                            if (mapping.node != nknode) {
                                mapping.node = 0;
                                ++multimap_count;
                                break; //exit -> multi-mapping read! TODO: allow mapping to consecutive nodes
                            } else {
                                mapping.last_pos = nk->second.pos;
                                ++mapping.unique_matches;
                            }
                        }
                    }
                }
                if (mapping.node != 0 and mapping.unique_matches >= min_matches) {
                    //optimisation: just save the mapping in a thread private collection for now, have a single thread putting from that into de structure at the end
                    private_results.push_back(mapping);
                    ++mapped_count;
                }
            }
            auto tc = ++total_count;
            if (tc % 10000000 == 0) sglib::OutputLog()<< mapped_count << " / " << tc <<" ("<<multimap_count<<" multi-mapped)"<< std::endl;
        }
    }
    for (auto & tres:thread_mapping_results){
        //sglib::OutputLog(sglib::LogLevels::DEBUG)<<"mixing in "<<tres.size()<<" thread specific results"<<std::endl;
        for (auto &rm:tres){
            read_to_node[rm.read_id] = rm.node;
            reads_in_node[rm.node].emplace_back(rm);
        }
        tres.clear();
        tres.shrink_to_fit();
    }
    uint64_t mapped_count=0,total_count=0,multimap_count=0;
    for (auto i=0;i<omp_get_max_threads();++i){
        mapped_count+=thread_mapped_count[i];
        total_count+=thread_total_count[i];
        multimap_count+=thread_multimap_count[i];
    }
    sglib::OutputLog(sglib::LogLevels::INFO)<<"Reads mapped: "<<mapped_count<<" / "<<total_count<<" ("<<multimap_count<<" multi-mapped)"<<std::endl;
#pragma omp parallel for
    for (sgNodeID_t n=1;n<reads_in_node.size();++n){
        std::sort(reads_in_node[n].begin(),reads_in_node[n].end());
    }
}

///**
// * @brief Mapping of paired end read files.
// *
// * Reads are mapped through a unique k-mer index in process_reads_from_file.
// * R1 and R2 are processed independently. R1 gets odds ids, R2 gets the next even id, so [1,2] and [3,4] are pairs.
// *
// * @todo fix and test 10x tags processing
// * @todo enable some basic 10x tag statistics
// * @todo add support for LMP reads (i.e FR reads)
// * @todo add distribution computing on the fly
// * @todo support other kind of indexes and variable k-mer size
// */
//void LinkedReadMapper::map_reads(std::string filename1, std::string filename2, prmReadType read_type, uint64_t max_mem) {
//    read1filename=std::move(filename1);
//    read2filename=std::move(filename2);
//    readType=read_type;
//    memlimit=max_mem;
//    remap_reads();
//}

void LinkedReadMapper::remove_obsolete_mappings(){
    uint64_t nodes=0,reads=0;
    std::set<sgNodeID_t> updated_nodes;
    for (auto n=1;n<sg.nodes.size();++n) {
        if (sg.nodes[n].status==sgNodeDeleted) {
            updated_nodes.insert(n);
            updated_nodes.insert(-n);
            reads_in_node[n > 0 ? n : -n].clear();
            ++nodes;
        }
    }
    for (auto &read_node:read_to_node) {
        if (updated_nodes.count(read_node) !=0 ) {
            read_node=0;
            ++reads;
        }
    }
    std::cout << "obsolete mappings removed from "<<nodes<<" nodes, total "<<reads<<" reads."<<std::endl;
}

std::set<bsg10xTag> LinkedReadMapper::get_node_tags(sgNodeID_t n) {
    std::set<bsg10xTag> tags;
    for (auto &rm:reads_in_node[(n>0?n:-n)])
        tags.insert(datastore.get_read_tag(rm.read_id));
    if (tags.count(0)>0) tags.erase(0);
    return tags;
}

std::map<bsg10xTag, std::vector<sgNodeID_t>> LinkedReadMapper::get_tag_nodes(uint32_t min_nodes,
                                                                             const std::vector<bool> &selected_nodes) {
    //Approach: node->tags->nodes (checks how many different tags join this node to every other node).

    std::map<bsg10xTag, std::vector<sgNodeID_t>> tag_nodes;
    bsg10xTag curr_tag=0,new_tag=0;
    std::set<sgNodeID_t> curr_nodes;
    sgNodeID_t curr_node;
    for (size_t i=1;i<read_to_node.size();++i){
        new_tag=datastore.get_read_tag(i);
        curr_node=llabs(read_to_node[i]);
        if (new_tag==curr_tag){
           if (curr_node!=0 and (selected_nodes.empty() or selected_nodes[curr_node]))
               curr_nodes.insert(curr_node);
        }
        else {
            if (curr_nodes.size()>=min_nodes and curr_tag>0){
                tag_nodes[curr_tag].reserve(curr_nodes.size());
                for (auto &n:curr_nodes)
                    tag_nodes[curr_tag].emplace_back(n);
            }
            curr_tag=new_tag;
            curr_nodes.clear();
            if (curr_node!=0 and (selected_nodes.empty() or selected_nodes[curr_node]))
                curr_nodes.insert(curr_node);
        }
    }
    if (curr_nodes.size()>=min_nodes and curr_tag>0){
        tag_nodes[curr_tag].reserve(curr_nodes.size());
        for (auto &n:curr_nodes)
            tag_nodes[curr_tag].emplace_back(n);
    }

    return tag_nodes;
}

std::vector<std::pair<sgNodeID_t , sgNodeID_t >> LinkedReadMapper::get_tag_neighbour_nodes(uint32_t min_shared,
                                                                                           const std::vector<bool> &selected_nodes) {
    std::vector<std::pair<sgNodeID_t , sgNodeID_t >> tns;

    auto nodes_in_tag= get_tag_nodes(2, selected_nodes);

    std::unordered_map<sgNodeID_t , std::vector<bsg10xTag>> tags_in_node;
    sglib::OutputLog()<<"There are "<<nodes_in_tag.size()<<" informative tags"<<std::endl;
    for (auto &t:nodes_in_tag){
        for (auto n:t.second) tags_in_node[n].push_back(t.first);
    }

    sglib::OutputLog()<<"all structures ready"<<std::endl;
#pragma omp parallel firstprivate(tags_in_node,nodes_in_tag)
    {
        std::vector<uint32_t> shared_with(sg.nodes.size());
        std::vector<std::pair<sgNodeID_t, sgNodeID_t >> tnsl;
#pragma omp for schedule(static,100)
        for (auto n = 1; n < sg.nodes.size(); ++n) {
            if (!selected_nodes[n]) continue;
            if (tags_in_node[n].size()<min_shared) continue;

            bzero(shared_with.data(), sizeof(uint32_t) * shared_with.size()); //clearing with bzero is the fastest way?
            for (auto t:tags_in_node[n])
                for (auto n:nodes_in_tag[t]) ++shared_with[n];
            for (auto i = n + 1; i < shared_with.size(); ++i) {
                if (shared_with[i] >= min_shared) tnsl.emplace_back(n, i);
            }

        }
#pragma omp critical
        tns.insert(tns.end(),tnsl.begin(),tnsl.end());
    }
    return tns;
}

/**
 * Finds all nodes that have tags that cover min_score of the total tags of each node.
 *
 * Example:
 *      - node A has tags 5, 5, 6, 7, 8
 *      - node B has tags 5, 8, 9, 9, 10
 *
 * Then B covers 3/6=0.5 of A tags, and A covers 2/5=0.4 of B tags.
 * If min_score=.5 then B is in A's neighbours, but A is not in B's
 *
 * Results are stored in the tag_neighbours vector
 * @param min_size
 * @param min_score
 */
void LinkedReadMapper::compute_all_tag_neighbours(int min_size, float min_score) {
    std::vector<std::unordered_map<sgNodeID_t,uint32_t>> scores(sg.nodes.size());
    bsg10xTag last_tag=0;
    //make a map with counts of how many times the tag appears on every node
    std::unordered_map<sgNodeID_t, uint32_t > node_readcount;
    sglib::OutputLog()<<"Counting into individual maps..."<<std::endl;
    for (size_t i=0;i<read_to_node.size();++i){
        if (datastore.get_read_tag(i)==0) continue;//skip tag 0
        if (datastore.get_read_tag(i)!=last_tag){
            //analyse tag per tag -> add this count on the shared set of this (check min_size for both)
            if (node_readcount.size()<500) {
                for (auto &n1:node_readcount) {
                    if (sg.nodes[n1.first].sequence.size() >= min_size and n1.second > 2) {
                        for (auto &n2:node_readcount) {
                            if (sg.nodes[n2.first].sequence.size() >= min_size and n2.second > 2) {
                                scores[n1.first][n2.first] += n1.second;
                            }
                        }
                    }
                }
            }
            //reset for next cycle
            last_tag=datastore.get_read_tag(i);
            node_readcount.clear();
        }
        if (read_to_node[i]!=0) {
            ++node_readcount[llabs(read_to_node[i])];
        }
    }
    //last tag
    for (auto &n1:node_readcount) {
        if (sg.nodes[n1.first].sequence.size()>=min_size) {
            for (auto &n2:node_readcount) {
                if (sg.nodes[n2.first].sequence.size() >= min_size) {
                    scores[n1.first][n2.first]+=n1.second;
                }
            }
        }
    }
    sglib::OutputLog()<<"... copying to result vector..."<<std::endl;
    //now flatten the map into its first dimension.
    tag_neighbours.clear();
    tag_neighbours.resize(sg.nodes.size());
    for (auto i=1;i<sg.nodes.size();++i){
        for (auto &s:scores[i]) {
            float tag_score = ((float) s.second) / scores[i][i];
            if (tag_score >= min_score)
                tag_neighbours[i].emplace_back(s.first, tag_score);
        }
    }
    sglib::OutputLog()<<"...DONE!"<<std::endl;
}

LinkedReadMapper LinkedReadMapper::operator=(const LinkedReadMapper &other) {
    if (&sg != &other.sg and &datastore != &other.datastore) { throw ("Can only copy paths from the same SequenceGraph"); }
    if (&other == this) {
        return *this;
    }
    reads_in_node = other.reads_in_node;
    read_to_node = other.read_to_node;
    return *this;
}


void LinkedReadMapper::write_tag_neighbours(std::string filename) {
    std::ofstream output_file(filename.c_str());
    uint64_t count;
    count =tag_neighbours.size();
    output_file.write((const char *) &count,sizeof(count));
    for (auto i=0;i<count;++i) {
        uint64_t mcount=tag_neighbours[i].size();
        output_file.write((const char *) &mcount,sizeof(mcount));
        output_file.write((const char *) tag_neighbours[i].data(), sizeof(TagNeighbour) * mcount);
    }
}

void LinkedReadMapper::read_tag_neighbours(std::string filename) {
    std::ifstream input_file(filename.c_str());
    uint64_t count;
    input_file.read(( char *) &count,sizeof(count));
    tag_neighbours.resize(count);
    for (auto i=0;i<count;++i) {
        uint64_t mcount;
        input_file.read(( char *) &mcount,sizeof(mcount));
        tag_neighbours[i].resize(mcount);
        input_file.read(( char *) tag_neighbours[i].data(), sizeof(TagNeighbour) * mcount);
    }
}