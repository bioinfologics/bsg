//
// Created by Bernardo Clavijo (EI) on 18/10/2017.
//


#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <cmath>
#include <list>
#include <queue>
#include <stack>
#include <tuple>
#include <functional>
#include <sdglib/graph/SequenceDistanceGraph.hpp>
#include <sdglib/mappers/LinkedReadMapper.hpp>

bool Node::is_canonical() {
    for (size_t i=0,j=sequence.size()-1;i<j;++i,--j){
        char f=sequence[i];
        char r=sequence[j];
        switch(r){
            case 'A':
                r='T';
                break;
            case 'C':
                r='G';
                break;
            case 'G':
                r='C';
                break;
            case 'T':
                r='A';
                break;
            case 'N':
                break;
            default:
                sdglib::OutputLog(sdglib::LogLevels::WARN) << "Unexpected character in fasta file: '" << r << "'" << std::endl;
        }
        if (f<r) return true;
        if (r<f) return false;
    }
    return true;
};

void Node::make_rc() {
    std::string rseq;
    rseq.resize(sequence.size());
    for (size_t i=0,j=sequence.size()-1;i<sequence.size();++i,--j){
        switch(sequence[j]){
            case 'A':
                rseq[i]='T';
                break;
            case 'C':
                rseq[i]='G';
                break;
            case 'G':
                rseq[i]='C';
                break;
            case 'T':
                rseq[i]='A';
                break;
        }
    }
    std::swap(sequence,rseq);
};

std::string SequenceDistanceGraph::get_node_sequence(sgNodeID_t n){
    if (n>0) return nodes[n].sequence;
    Node rc(nodes[n].sequence);
    rc.make_rc();
    return rc.sequence;
}

uint64_t SequenceDistanceGraph::get_node_size(sgNodeID_t n) {
    return nodes[llabs(n)].sequence.size();
}

bool SequenceDistanceGraph::is_sane() const {
    for (auto n=0;n<nodes.size();++n){
        for (auto l:links[n]){
            bool found=false;
            for (auto &dl:links[llabs(l.dest)]) {
                if (dl.dest == l.source and dl.source == l.dest){
                    found=true;
                    break;
                }
            }
            if (!found) return false;
        }
    }
    return true;
}

sgNodeID_t SequenceDistanceGraph::add_node(Node n) {
    nodes.emplace_back(n);
    links.emplace_back();
    return (sgNodeID_t) nodes.size()-1;
}

void SequenceDistanceGraph::remove_node(sgNodeID_t n) {
    sgNodeID_t node=(n>0? n:-n);
    auto oldlinks=links[node];//this creates a copy to allow the iteration
    for (auto &l:oldlinks) remove_link(l.source,l.dest);
    nodes[node].status=sgNodeDeleted;
    //TODO: this is a lazy solution
    nodes[node].sequence.clear();
    //TODO: remove read mappings
}

Link SequenceDistanceGraph::get_link(sgNodeID_t source, sgNodeID_t dest) {
    for (auto l:links[(source > 0 ? source : -source)]) {
        if (l.source==source,l.dest==dest) return l;
    }
    return Link(0,0,0);
}

size_t SequenceDistanceGraph::count_active_nodes() {
    size_t t = 0;
    for (auto &n: nodes) {
        if (n.status == sgNodeStatus_t::sgNodeActive) ++t;
    }
    return t;
}

bool Link::operator==(const Link a){
    if (a.source == this->source && a.dest == this->dest){
        return true;
    }
    return false;
}

bool Link::operator<(const Link a) const {
    if (a.source < this->source){
        return true;
    } if (a.source == this->source && a.dest < this->dest) {
        return  true;
    }
    return false;
}


std::vector<std::vector<sgNodeID_t>> SequenceDistanceGraph::connected_components(int max_nr_totalinks, int max_nr_dirlinks,
                                                                         int min_rsize) {
    std::vector<bool> used(nodes.size());
    std::vector<std::vector<sgNodeID_t>> components;
    //TODO: first find all repeats, add them as independent components and mark them as used.
    size_t max_component = 0;
    for (sgNodeID_t start_node=1;start_node<nodes.size();++start_node){
        if (nodes[start_node].status==sgNodeDeleted) continue;
        if (false==used[start_node]){
            used[start_node]=true;
            //if start node is repeat, just add a single-node component
            std::set<sgNodeID_t> in_component;
            in_component.insert(start_node);
            std::set<sgNodeID_t> to_explore;
            to_explore.insert(start_node);
            while(to_explore.size()){
                auto n=*to_explore.begin();
                to_explore.erase(n);
                //find all connections that are not already used, include both in component and in to_explore
                for (auto l:links[n]) {
                    sgNodeID_t next=(l.dest>0 ? l.dest : -l.dest);
                    if (in_component.count(next)==0) {
                        in_component.insert(next);
                        to_explore.insert(next);
                    }
                }
            }
            components.emplace_back();
            for (sgNodeID_t n:in_component){
                components.back().push_back(n);
                used[n]=true;
            }
            if (in_component.size() > max_component){
                max_component = in_component.size();
            }
        }
    }
    std::cout << "Max component size: " << max_component << std::endl;
    return components;
}

void SequenceDistanceGraph::write(std::ofstream & output_file) {
    uint64_t count;
    count=nodes.size();
    output_file.write((char *) &count,sizeof(count));
    for (auto &n:nodes){
        output_file.write((char *) &n.status,sizeof(n.status));
        count=n.sequence.size();
        output_file.write((char *) &count,sizeof(count));
        output_file.write((char *) n.sequence.data(),count);
    }
    count=links.size();
    output_file.write((char *) &count,sizeof(count));
    for (auto nl:links) {
        uint64_t lcount=nl.size();
        output_file.write((char *) &lcount,sizeof(lcount));
        output_file.write((char *) nl.data(), sizeof(Link) * lcount);
    }
}

void SequenceDistanceGraph::read(std::ifstream & input_file) {
    uint64_t count;
    input_file.read((char *) &count,sizeof(count));
    nodes.clear();
    nodes.reserve(count);
    uint64_t active=0;
    for (auto i=0;i<count;++i){
        uint64_t seqsize;
        std::string seq;
        sgNodeStatus_t status;
        input_file.read((char *) &status,sizeof(status));
        input_file.read((char *) &seqsize,sizeof(seqsize));
        seq.resize(seqsize);
        input_file.read((char *) seq.data(),seqsize);
        nodes.emplace_back(seq);
        nodes.back().status=status;
        if (nodes.back().status==sgNodeStatus_t::sgNodeActive) ++active;
    }
    input_file.read((char *) &count,sizeof(count));
    links.resize(count);
    for (auto &nl:links) {
        uint64_t lcount;
        input_file.read((char *) &lcount,sizeof(lcount));
        nl.resize(lcount,{0,0,0});
        input_file.read((char *) nl.data(), sizeof(Link) * lcount);
    }
}

void SequenceDistanceGraph::load_from_gfa(std::string filename) {
    std::string line;
    this->filename=filename;
    //check the filename ends in .gfa
    if (filename.size()>4 and filename.substr(filename.size()-4,4)==".gfa"){
        this->fasta_filename=filename.substr(0,filename.size()-4)+".fasta";
    }
    else throw std::invalid_argument("Filename of the gfa input does not end in gfa, it ends in '"+filename.substr(filename.size()-4,4)+"'");

    std::ifstream gfaf(filename);
    if (!gfaf) throw std::invalid_argument("Can't read gfa file");
    if (gfaf.peek() == std::ifstream::traits_type::eof()) throw std::invalid_argument("Empty gfa file");

    std::getline(gfaf, line);
    if (line!="H\tVN:Z:1.0") std::cout<<"WARNING, first line of gfa doesn't correspond to GFA1"<<std::endl;

    std::ifstream fastaf(fasta_filename);
    sdglib::OutputLog(sdglib::LogLevels::INFO) << "Graph fasta filesname: " << fasta_filename << std::endl;
    if (!fastaf) throw std::invalid_argument("Can't read graph fasta file");
    if (fastaf.peek() == std::ifstream::traits_type::eof()) throw std::invalid_argument("Empty fasta file");

    //load all sequences from fasta file if they're not canonical, flip and remember they're flipped
    sdglib::OutputLog(sdglib::LogLevels::INFO) << "Loading sequences from " << fasta_filename << std::endl;

    std::string name, seq = "";
    seq.reserve(10000000); //stupid hack but probably useful to reserve
    oldnames_to_ids.clear();
    oldnames.push_back("");
    nodes.clear();
    links.clear();
    add_node(Node("",sgNodeStatus_t::sgNodeDeleted)); //an empty deleted node on 0, just to skip the space
    sgNodeID_t nextid=1;
    uint64_t rcnodes=0;
    while(!fastaf.eof()){
        std::getline(fastaf,line);
        if (fastaf.eof() or line[0] == '>'){

            if (!name.empty()) {
                //rough ansi C and C++ mix but it works
                if (oldnames_to_ids.find(name) != oldnames_to_ids.end())
                    throw std::logic_error("sequence " + name + " is already defined");
                oldnames_to_ids[name] = add_node(Node(seq));
                oldnames.push_back(name);
                //reverse seq if not canonical
                if (!nodes.back().is_canonical()) {
                    nodes.back().make_rc();
                    oldnames_to_ids[name] = -oldnames_to_ids[name];
                    ++rcnodes;
                }
            }

            // Clear the name and set name to the new name, this is a new sequence!
            name.clear();
            for (auto i = 1; i < line.size() and line[i] != ' '; ++i) name += line[i];
            seq = "";
        } else {
            seq += line;

        }
    }
    sdglib::OutputLog(sdglib::LogLevels::INFO) << nodes.size()-1 << " nodes loaded (" << rcnodes << " canonised)." << std::endl;

    //load store all connections.

    std::string gfa_rtype,gfa_source,gfa_sourcedir,gfa_dest,gfa_destdir,gfa_cigar,gfa_star,gfa_length;
    sgNodeID_t src_id,dest_id;
    int32_t dist;
    uint64_t lcount=0;
    uint64_t dist_egt0(0);
    while(std::getline(gfaf, line) and !gfaf.eof()) {
        std::istringstream iss(line);
        iss >> gfa_rtype;

        if (gfa_rtype == "S"){
            iss >> gfa_source;
            iss >> gfa_star;
            iss >> gfa_length; // parse to number

            if (gfa_star != "*") {
                throw std::logic_error("Sequences should be in a separate file.");
            }

            // Check equal length seq and node length reported in gfa
            if (oldnames_to_ids.find(gfa_source) != oldnames_to_ids.end()){
                if (std::stoi(gfa_length.substr(5)) != nodes[std::abs(oldnames_to_ids[gfa_source])].sequence.length()){
                    throw std::logic_error("Different length in node and fasta for sequence: " + gfa_source+ " -> gfa:" + gfa_length.substr(5) + ", fasta: " + std::to_string(nodes[oldnames_to_ids[gfa_source]].sequence.length()));
                }
            }
        } else if (gfa_rtype == "L"){
            iss >> gfa_source;
            iss >> gfa_sourcedir;
            iss >> gfa_dest;
            iss >> gfa_destdir;
            iss >> gfa_cigar;
            //std::cout<<"'"<<source<<"' '"<<gfa_sourcedir<<"' '"<<dest<<"' '"<<destdir<<"'"<<std::endl;
            if (oldnames_to_ids.find(gfa_source) == oldnames_to_ids.end()){
                oldnames_to_ids[gfa_source] = add_node(Node(""));
                //std::cout<<"added source!" <<source<<std::endl;
            }
            if (oldnames_to_ids.find(gfa_dest) == oldnames_to_ids.end()){
                oldnames_to_ids[gfa_dest] = add_node(Node(""));
                //std::cout<<"added dest! "<<dest<<std::endl;
            }
            src_id=oldnames_to_ids[gfa_source];
            dest_id=oldnames_to_ids[gfa_dest];

            //Go from GFA's "Links as paths" to a normal "nodes have sinks (-/start/left) and sources (+/end/right)
            if (gfa_sourcedir == "+") src_id=-src_id;
            if (gfa_destdir == "-") dest_id=-dest_id;
            dist=0;
            if (gfa_cigar.size()>1 and gfa_cigar[gfa_cigar.size()-1]=='M') {
                //TODO: better checks for M
                dist=-atoi(gfa_cigar.c_str());
                if (dist>=0) {
                    dist_egt0++;
                }
            }
            add_link(src_id,dest_id,dist);
            ++lcount;
        }
    }
    if (dist_egt0 > lcount*0.5f) {
        sdglib::OutputLog(sdglib::LogLevels::WARN) << "Warning: The loaded graph contains " << dist_egt0 << " non-overlapping links out of " << lcount << std::endl;
    }
    sdglib::OutputLog(sdglib::LogLevels::INFO) << nodes.size() - 1 << " nodes after connecting with " << lcount << " links." << std::endl;
}

void SequenceDistanceGraph::load_from_fasta(std::string filename) {
    std::string line;
    this->filename=filename;
    this->fasta_filename=filename;

    std::ifstream fastaf(fasta_filename);
    sdglib::OutputLog(sdglib::LogLevels::INFO) << "Graph fasta filesname: " << fasta_filename << std::endl;
    if (!fastaf) throw std::invalid_argument("Can't read graph fasta file");
    if (fastaf.peek() == std::ifstream::traits_type::eof()) throw std::invalid_argument("Empty fasta file");

    //load all sequences from fasta file if they're not canonical, flip and remember they're flipped
    sdglib::OutputLog(sdglib::LogLevels::INFO) << "Loading sequences from " << fasta_filename << std::endl;

    std::string name, seq = "";
    seq.reserve(10000000); //stupid hack but probably useful to reserve
    oldnames_to_ids.clear();
    oldnames.push_back("");
    nodes.clear();
    links.clear();
    add_node(Node("",sgNodeStatus_t::sgNodeDeleted)); //an empty deleted node on 0, just to skip the space
    uint64_t rcnodes=0;
    while(!fastaf.eof()){
        std::getline(fastaf,line);
        if (fastaf.eof() or line[0] == '>'){

            if (!name.empty()) {
                //rough ansi C and C++ mix but it works
                if (oldnames_to_ids.find(name) != oldnames_to_ids.end())
                    throw std::logic_error("sequence " + name + " is already defined");
                oldnames_to_ids[name] = add_node(Node(seq));
                oldnames.push_back(name);
            }

            // Clear the name and set name to the new name, this is a new sequence!
            name.clear();
            for (auto i = 1; i < line.size() and line[i] != ' '; ++i) name += line[i];
            seq = "";
        } else {
            seq += line;

        }
    }
    sdglib::OutputLog(sdglib::LogLevels::INFO) << nodes.size()-1 << " nodes loaded " << std::endl;
}

void SequenceDistanceGraph::write_to_gfa(std::string filename, const std::vector<std::vector<Link>> &arg_links,
                                 const std::vector<sgNodeID_t> &selected_nodes, const std::vector<sgNodeID_t> &mark_red,
                                 const std::vector<double> &depths) {
    std::unordered_set<sgNodeID_t> output_mark_red(mark_red.begin(), mark_red.end());
    std::unordered_set<sgNodeID_t > output_nodes(selected_nodes.begin(), selected_nodes.end());
    std::string fasta_filename;
    //check the filename ends in .gfa
    if (filename.size()>4 and filename.substr(filename.size()-4,4)==".gfa"){
        fasta_filename=filename.substr(0,filename.size()-4)+".fasta";
    }
    else throw std::invalid_argument("filename of the gfa input does not end in gfa, it ends in '"+filename.substr(filename.size()-4,4)+"'");

    std::ofstream gfaf(filename);
    if (!gfaf) throw std::invalid_argument("Can't write to gfa file");
    gfaf<<"H\tVN:Z:1.0"<<std::endl;

    std::ofstream fastaf(fasta_filename);
    if (!fastaf) throw std::invalid_argument("Can't write to fasta file");


    //load all sequences from fasta file if they're not canonical, flip and remember they're flipped
    //std::cout<<"Writing sequences to "<<fasta_filename<<std::endl;

    for (sgNodeID_t i=1;i<nodes.size();++i){
        if (nodes[i].status==sgNodeDeleted) continue;
        if (!output_nodes.empty() and output_nodes.count(i)==0 and output_nodes.count(-i)==0) continue;
        fastaf<<">seq"<<i<<std::endl<<nodes[i].sequence<<std::endl;
        gfaf<<"S\tseq"<<i<<"\t*\tLN:i:"<<nodes[i].sequence.size()<<"\tUR:Z:"<<fasta_filename
                <<(output_mark_red.count(i)?"\tCL:Z:red":"")<<(depths.empty() or std::isnan(depths[i])?"":"\tDP:f:"+std::to_string(depths[i]))<<std::endl;
    }

    for (auto &ls:(arg_links.size()>0? arg_links:links)){
        for (auto &l:ls)
            if (l.source<=l.dest and (output_nodes.empty() or
                    output_nodes.count(l.source)>0 or output_nodes.count(-l.source)>0 or
                    output_nodes.count(l.dest)>0 or output_nodes.count(-l.dest)>0)) {
                gfaf<<"L\t";
                if (l.source>0) gfaf<<"seq"<<l.source<<"\t-\t";
                else gfaf<<"seq"<<-l.source<<"\t+\t";
                if (l.dest>0) gfaf<<"seq"<<l.dest<<"\t+\t";
                else gfaf<<"seq"<<-l.dest<<"\t-\t";
                gfaf<<(l.dist<0 ? -l.dist : 0)<<"M"<<std::endl;
            }
    }

}

std::vector<sgNodeID_t> SequenceDistanceGraph::oldnames_to_nodes(std::string _oldnames) {
    std::vector<sgNodeID_t> nv;
    const char * s = _oldnames.c_str();
    const char * sign;
    std::string oldname;
    sgNodeID_t node;
    while (*s!= NULL){
        for (sign=s;*sign!=NULL and *sign !='+' and *sign!='-';++sign);
        if (sign ==s or *sign==NULL ) throw std::invalid_argument("invalid path specification");
        oldname=s;
        oldname.resize(sign-s);
        node=oldnames_to_ids[oldname];
        if (0==node) throw std::invalid_argument("node "+oldname+" doesn't exist in graph");
        if (*sign=='-') node=-node;
        nv.push_back(node);
        s=sign+1;
        if (*s==',') ++s;
        else if (*s!=NULL) throw std::invalid_argument("invalid path specification");

    }
    return nv;
}

std::vector<SequenceGraphPath> SequenceDistanceGraph::get_all_unitigs(uint16_t min_nodes) {
    std::vector<SequenceGraphPath> unitigs;
    std::vector<bool> used(nodes.size(),false);

    for (auto n=1;n<nodes.size();++n){
        if (used[n] or nodes[n].status==sgNodeDeleted) continue;
        used[n]=true;
        SequenceGraphPath path(*this,{n});

        //two passes: 0->fw, 1->bw, path is inverted twice, so still n is +
        for (auto pass=0; pass<2; ++pass) {
            //walk til a "non-unitig" junction
            for (auto fn = get_fw_links(path.nodes.back()); fn.size() == 1; fn = get_fw_links(path.nodes.back())) {
                if (!used[llabs(fn[0].dest)] and get_bw_links(fn[0].dest).size() == 1) {
                    path.nodes.emplace_back(fn[0].dest);
                    used[fn[0].dest > 0 ? fn[0].dest : -fn[0].dest] = true;
                } else break;
            }
            path.reverse();
        }
        if (path.nodes.size()>=min_nodes) unitigs.push_back(path);
    }
    return unitigs;
}

uint32_t SequenceDistanceGraph::join_all_unitigs() {
    uint32_t joined=0;
    for (auto p:get_all_unitigs(2)){
        join_path(p);
        ++joined;
    }
    return joined;
}

void SequenceDistanceGraph::join_path(SequenceGraphPath p, bool consume_nodes) {
    std::set<sgNodeID_t> pnodes;
    for (auto n:p.nodes) {
        pnodes.insert( n );
        pnodes.insert( -n );
    }

    if (!p.is_canonical()) p.reverse();
    sgNodeID_t new_node=add_node(Node(p.get_sequence()));
    //TODO:check, this may have a problem with a circle
    for (auto l:get_bw_links(p.nodes.front())) add_link(new_node,l.dest,l.dist);
    for (auto l:get_fw_links(p.nodes.back())) add_link(-new_node,l.dest,l.dist);

    //TODO: update read mappings
    if (consume_nodes) {
        for (auto n:p.nodes) {
            //check if the node has neighbours not included in the path.
            bool ext_neigh=false;
            if (n!=p.nodes.back()) for (auto l:get_fw_links(n)) if (pnodes.count(l.dest)==0) ext_neigh=true;
            if (n!=p.nodes.front()) for (auto l:get_bw_links(n)) if (pnodes.count(l.dest)==0) ext_neigh=true;
            if (ext_neigh) continue;
            remove_node(n);
        }
    }
}

std::vector<nodeVisitor>
SequenceDistanceGraph::breath_first_search(const nodeVisitor seed, unsigned int size_limit, unsigned int edge_limit, std::set<nodeVisitor> tabu) {
    std::queue<nodeVisitor> to_visit;
    to_visit.push(seed);
    std::set<nodeVisitor> visited_set;
    for (const auto &n:tabu) {
        visited_set.insert(n);
    }

    while (!to_visit.empty()) {
        const auto activeNode(to_visit.front());
        to_visit.pop();
        auto looker = nodeVisitor(activeNode.node,0,0);
        auto visitedNode(visited_set.find( looker ) );
        if (visitedNode == visited_set.end() and    // Active node has not been visited
            (activeNode.path_length < edge_limit or edge_limit==0) and  // Is within path limits
            (activeNode.dist < size_limit or size_limit==0) ) // Is within sequence limits
        {
            visited_set.insert(nodeVisitor(activeNode.node, activeNode.dist, activeNode.path_length));  // Mark as visited
            for (const auto &l: get_fw_links(activeNode.node)) {    // Visit all its neighbours
                to_visit.push(nodeVisitor(l.dest,
                                          static_cast<uint>(activeNode.dist + nodes[l.dest > 0 ? l.dest : -l.dest].sequence.length()),
                                          activeNode.path_length+1));
            }
        } else if (visitedNode != visited_set.end() and // If the node has been visited
                   (visitedNode->path_length > activeNode.path_length or visitedNode->dist > activeNode.dist)) // but is closer than the last time I saw it
        {
            visited_set.erase(visitedNode); // Remove it from the visited nodes and
            to_visit.push(nodeVisitor(activeNode.node, activeNode.dist, activeNode.path_length)); // add it to the list of nodes to visit
        }
    }

    std::vector<nodeVisitor > result;
    for (const auto &v:visited_set) {
        if (v.path_length == 0) continue; // If node is seed, do not add it
        result.push_back(v);
    }
    return result;
}

std::vector<nodeVisitor>
SequenceDistanceGraph::depth_first_search(const nodeVisitor seed, unsigned int size_limit, unsigned int edge_limit, std::set<nodeVisitor> tabu) {
    // Create a stack with the nodes and the path length

    std::stack<nodeVisitor> to_visit;
    to_visit.push(seed);
    std::set<nodeVisitor> visited_set;
    for (const auto &n:tabu) {
        visited_set.insert(n);
    }

    while (!to_visit.empty()) {
        const auto activeNode(to_visit.top());
        to_visit.pop();
        auto looker = nodeVisitor(activeNode.node,0,0);
        auto visitedNode(visited_set.find( looker ) );
        if (visitedNode == visited_set.end() and    // Active node has not been visited
                (activeNode.path_length < edge_limit or edge_limit==0) and  // Is within path limits
                (activeNode.dist < size_limit or size_limit==0) ) // Is within sequence limits
        {
            visited_set.insert(nodeVisitor(activeNode.node, activeNode.dist, activeNode.path_length));  // Mark as visited
            for (const auto &l: get_fw_links(activeNode.node)) {    // Visit all its neighbours
                to_visit.push(nodeVisitor(l.dest,
                                 static_cast<uint>(activeNode.dist + nodes[l.dest > 0 ? l.dest : -l.dest].sequence.length()),
                                 activeNode.path_length+1));
            }
        } else if (visitedNode != visited_set.end() and // If the node has been visited
                (visitedNode->path_length > activeNode.path_length or visitedNode->dist > activeNode.dist)) // but is closer than the last time I saw it
        {
            visited_set.erase(visitedNode); // Remove it from the visited nodes and
            to_visit.push(nodeVisitor(activeNode.node, activeNode.dist, activeNode.path_length)); // add it to the list of nodes to visit
        }
    }

    std::vector<nodeVisitor > result;
    for (const auto &v:visited_set) {
        if (v.path_length == 0) continue; // If node is seed, do not add it
        result.push_back(v);
    }
    return result;
}

std::vector<nodeVisitor>
SequenceDistanceGraph::explore_nodes(std::vector<std::string> &nodes, unsigned int size_limit, unsigned int edge_limit) {
    std::set<nodeVisitor> resultNodes;
    // For each node in the list
    for (const auto &n:nodes) {
        auto id = oldnames_to_ids.at(n);
        std::set<nodeVisitor> results;
        results.emplace(id, 0, 0);
        results.emplace(-id, 0, 0);
        do {
            sdglib::OutputLog(sdglib::DEBUG,false) << "Nodes to visit:" << std::endl;
            std::copy(results.begin(), results.end(), std::ostream_iterator<nodeVisitor>(sdglib::OutputLog(sdglib::DEBUG, false), ","));
            sdglib::OutputLog(sdglib::DEBUG,false) << std::endl;
            sdglib::OutputLog(sdglib::DEBUG,false) << "Nodes visited:" << std::endl;
            std::copy(resultNodes.begin(), resultNodes.end(), std::ostream_iterator<nodeVisitor>(sdglib::OutputLog(sdglib::DEBUG, false), ","));
            sdglib::OutputLog(sdglib::DEBUG,false) << std::endl;
            auto toVisit = *results.begin();
            results.erase(toVisit);
            // Explore node forward
            auto visitedNodes = depth_first_search(toVisit, size_limit, edge_limit, resultNodes);

            // For each visited node
            for (const auto &node:visitedNodes) {
                // If is the same as the node I am visiting, do nothing
                if (node == toVisit) continue;

                auto visitedNode(resultNodes.find(node));

                // If was in previous results
                if (visitedNode != resultNodes.end()) {
                    // If now has a shorter path or closer distance
                    if (node.path_length > visitedNode->path_length or node.dist > visitedNode->dist) {
                        resultNodes.erase(visitedNode);
                    } else {
                        continue;
                    }
                }
                results.insert(node);
                nodeVisitor rev = nodeVisitor(-1*node.node, node.dist, node.path_length);
                results.insert(rev);
                resultNodes.emplace(node);
            }
            // While exploration results > 0 explore resulting nodes
        } while (!results.empty());
    }
    return std::vector<nodeVisitor>(resultNodes.begin(), resultNodes.end());
}





void SequenceDistanceGraph::expand_node(sgNodeID_t nodeID, std::vector<std::vector<sgNodeID_t>> bw,
                                std::vector<std::vector<sgNodeID_t>> fw) {
    if (nodeID<0) {
        //std::cout<<"WARNING: ERROR: expand_node only accepts positive nodes!"<<std::endl;
        //return;
        nodeID=-nodeID;
        std::swap(bw,fw);
        for (auto &x:bw) for (auto &xx:x) xx=-xx;
        for (auto &x:fw) for (auto &xx:x) xx=-xx;
    }
    //TODO: check all inputs are included in bw and all outputs are included in fw, only once
    auto orig_links=links[nodeID];
    std::vector<std::vector<Link>> new_links;
    new_links.resize(bw.size());
    for (auto l:orig_links){
        for (auto in=0;in<bw.size();++in){
            if (l.source==nodeID and std::find(bw[in].begin(),bw[in].end(),l.dest)!=bw[in].end()) {
                new_links[in].push_back(l);
                break;
            }
        }
        for (auto in=0;in<fw.size();++in){
            if (l.source==-nodeID and std::find(fw[in].begin(),fw[in].end(),l.dest)!=fw[in].end()) {
                new_links[in].push_back(l);
                break;
            }
        }
    }

    //Create all extra copies of the node.
    for (auto in=0;in<new_links.size();++in){
        auto new_node=add_node(Node(nodes[llabs(nodeID)].sequence));
        for (auto l:new_links[in]) {
            add_link((l.source>0 ? new_node:-new_node),l.dest,l.dist);
        }
    }
    remove_node(nodeID);

}

std::vector<std::pair<sgNodeID_t,int64_t>> SequenceDistanceGraph::get_distances_to(sgNodeID_t n,
                                                                            std::set<sgNodeID_t> destinations,
                                                                            int64_t max_dist) {
    std::vector<std::pair<sgNodeID_t,int64_t>> current_nodes,next_nodes,final_nodes;
    current_nodes.push_back({n,-nodes[llabs(n)].sequence.size()});
    int rounds=20;
    while (not current_nodes.empty() and --rounds>0){
        //std::cout<<"starting round of context expansion, current nodes:  ";
        //for (auto cn:current_nodes) std::cout<<cn.first<<"("<<cn.second<<")  ";
        //std::cout<<std::endl;
        next_nodes.clear();
        for (auto nd:current_nodes){
            //if node is a destination add it to final nodes
            if (destinations.count(nd.first)>0 or destinations.count(-nd.first)>0){
                final_nodes.push_back(nd);
            }
            //else get fw links, compute distances for each, and if not >max_dist add to nodes
            else {
                for (auto l:get_fw_links(nd.first)){
                    int64_t dist=nd.second;
                    dist+=nodes[llabs(nd.first)].sequence.size();
                    dist+=l.dist;
                    //std::cout<<"candidate next node "<<l.dest<<" at distance "<<dist<<std::endl;
                    if (dist<=max_dist) next_nodes.push_back({l.dest,dist});
                }
            }
        }
        current_nodes=next_nodes;
    }
    return final_nodes;
}

std::vector<SequenceSubGraph> SequenceDistanceGraph::get_all_bubbly_subgraphs(uint32_t maxsubgraphs) {
    std::vector<SequenceSubGraph> subgraphs;
    std::vector<bool> used(nodes.size(),false);
    /*
     * the loop always keep the first and the last elements as c=2 collapsed nodes.
     * it starts with a c=2 node, and goes thorugh all bubbles fw, then reverts the subgraph and repeats
     */
    SequenceSubGraph subgraph(*this);
    for (auto n=1;n<nodes.size();++n){
        if (used[n] or nodes[n].status==sgNodeDeleted) continue;
        subgraph.nodes.clear();

        subgraph.nodes.push_back(n);
        bool circular=false;
        //two passes: 0->fw, 1->bw, path is inverted twice, so still n is +
        for (auto pass=0; pass<2; ++pass) {
            //while there's a possible bubble fw.
            for (auto fn = get_fw_links(subgraph.nodes.back()); fn.size() == 2; fn = get_fw_links(subgraph.nodes.back())) {
                //if it is not a real bubble, get out.
                if (get_bw_links(fn[0].dest).size()!=1 or get_bw_links(fn[0].dest).size()!=1) break;
                auto fl1=get_fw_links(fn[0].dest);
                if (fl1.size()!=1) break;
                auto fl2=get_fw_links(fn[1].dest);
                if (fl2.size()!=1) break;
                if (fl2[0].dest!=fl1[0].dest) break;
                auto next_end=fl2[0].dest;
                //all conditions met, update subgraph
                if (used[llabs(fn[0].dest)] or used[llabs(fn[1].dest)] or used[llabs(next_end)]) {
                    circular=true;
                    break;
                }
                subgraph.nodes.push_back(fn[0].dest);
                subgraph.nodes.push_back(fn[1].dest);
                subgraph.nodes.push_back(next_end);
                used[llabs(fn[0].dest)]=true;
                used[llabs(fn[1].dest)]=true;
                used[llabs(next_end)]=true;
                used[n]=true;
            }
            SequenceSubGraph new_subgraph(*this);
            for (auto it=subgraph.nodes.rbegin();it<subgraph.nodes.rend();++it) new_subgraph.nodes.push_back(-*it);
            std::swap(new_subgraph.nodes,subgraph.nodes);
        }
        if (subgraph.nodes.size()>6 and not circular) {
            subgraphs.push_back(subgraph);
            if (subgraphs.size()==maxsubgraphs) break;
            //std::cout<<"Bubbly path found: ";
            //for (auto &n:subgraph) std::cout<<"  "<<n<<" ("<<sdg.nodes[(n>0?n:-n)].sequence.size()<<"bp)";
            //std::cout<<std::endl;
        }
    }
    return subgraphs;
}

void SequenceDistanceGraph::print_bubbly_subgraph_stats(const std::vector<SequenceSubGraph> &bubbly_paths) {
    std::vector<uint64_t> solved_sizes,original_sizes;
    sdglib::OutputLog()<<bubbly_paths.size()<<" bubbly paths"<<std::endl;
    auto &log_no_date=sdglib::OutputLog(sdglib::LogLevels::INFO,false);
    uint64_t total_size=0,total_solved_size=0;
    for (const auto &bp:bubbly_paths){
        total_size+=bp.total_size();
        SequenceGraphPath p1(*this),p2(*this);
        original_sizes.push_back(nodes[llabs(bp.nodes.front())].sequence.size());
        original_sizes.push_back(nodes[llabs(bp.nodes.back())].sequence.size());
        solved_sizes.push_back(nodes[llabs(bp.nodes.front())].sequence.size());
        total_solved_size+=solved_sizes.back();
        solved_sizes.push_back(nodes[llabs(bp.nodes.back())].sequence.size());
        total_solved_size+=solved_sizes.back();
        for (auto i=1; i<bp.nodes.size()-1; ++i){
            original_sizes.push_back(nodes[llabs(bp.nodes[i])].sequence.size());
            if (i%3==0){
                p1.nodes.push_back(bp.nodes[i]);
                p2.nodes.push_back(bp.nodes[i]);
            } else if (i%3==1) p1.nodes.push_back(bp.nodes[i]);
            else p2.nodes.push_back(bp.nodes[i]);
        }
        solved_sizes.push_back(p1.get_sequence().size());
        total_solved_size+=solved_sizes.back();
        solved_sizes.push_back(p2.get_sequence().size());
        total_solved_size+=solved_sizes.back();
    }
    std::sort(solved_sizes.rbegin(),solved_sizes.rend());
    std::sort(original_sizes.rbegin(),original_sizes.rend());
    auto on20s=total_size*.2;
    auto on50s=total_size*.5;
    auto on80s=total_size*.8;
    sdglib::OutputLog() <<"Currently "<<original_sizes.size()<<" sequences with "<<total_size<<"bp, ";
    uint64_t acc=0;
    for (auto s:original_sizes){
        if (acc<on20s and acc+s>on20s) log_no_date<<"N20: "<<s<<"  ";
        if (acc<on50s and acc+s>on50s) log_no_date<<"N50: "<<s<<"  ";
        if (acc<on80s and acc+s>on80s) log_no_date<<"N80: "<<s<<"  ";
        acc+=s;
    }
    log_no_date<<std::endl;
    auto sn20s=total_solved_size*.2;
    auto sn50s=total_solved_size*.5;
    auto sn80s=total_solved_size*.8;
    sdglib::OutputLog()<<"Potentially "<<solved_sizes.size()<<" sequences with "<<total_solved_size<<"bp, ";
    acc=0;
    for (auto s:solved_sizes){
        if (acc<sn20s and acc+s>sn20s) log_no_date<<"N20: "<<s<<"  ";
        if (acc<sn50s and acc+s>sn50s) log_no_date<<"N50: "<<s<<"  ";
        if (acc<sn80s and acc+s>sn80s) log_no_date<<"N80: "<<s<<"  ";
        acc+=s;
    }
    log_no_date<<std::endl;

}

std::vector<SequenceGraphPath> SequenceDistanceGraph::find_all_paths_between(sgNodeID_t from,sgNodeID_t to, int64_t max_size, int max_nodes, bool abort_on_loops) const {
    typedef struct T {
        int64_t prev;
        sgNodeID_t node;
        int node_count;
        int64_t partial_size;
        T(uint64_t a, sgNodeID_t b, int c, int64_t d) : prev(a),node(b),node_count(c),partial_size(d){};
    } pathNodeEntry_t;

    std::vector<pathNodeEntry_t> node_entries;
    node_entries.reserve(100000);
    std::vector<SequenceGraphPath> final_paths;
    std::vector<sgNodeID_t> pp;

    for(auto &fl:get_fw_links(from)) {
        if (nodes[llabs(fl.dest)].sequence.size()<=max_size) {
            node_entries.emplace_back(-1, fl.dest, 1, nodes[llabs(fl.dest)].sequence.size());
        }
    }


    // XXX: This loop is growing forever on node 2083801 for backbone 58, the limits are heuristics to cap the complexity of regions captured
    for (uint64_t current_index=0;current_index<node_entries.size() and node_entries.size() < 1000001 and final_paths.size() < 1001;++current_index){
        auto current_entry=node_entries[current_index];
        auto fwl=get_fw_links(current_entry.node);
        for(auto &fl:fwl) {
            if (fl.dest==to){
                //reconstruct path backwards
                pp.resize(current_entry.node_count);

                for (uint64_t ei=current_index;ei!=-1;ei=node_entries[ei].prev){
                    pp[node_entries[ei].node_count-1]=node_entries[ei].node;
                }
                //check if there are any loops
                if (abort_on_loops) {
                    for (auto i1 = 0; i1 < pp.size(); ++i1) {
                        for (auto i2 = 0; i2 < pp.size(); ++i2) {
                            if (pp[i1]==pp[i2]) {
                                return {};
                            }
                        }
                    }
                }
                //add to solutions
                final_paths.emplace_back(*this,pp);
            }
            else {
                uint64_t new_size=current_entry.partial_size+fl.dist+nodes[llabs(fl.dest)].sequence.size();
                if (new_size<=max_size and current_entry.node_count<=max_nodes) {
                    node_entries.emplace_back(current_index, fl.dest, current_entry.node_count+1, new_size);
                }
            }
        }

    }

    bool early_exit=false;
    if (node_entries.size() == 1000000) {
        std::cout << "From " << from << " to " << to << " with max_size " << max_size << " and max_nodes " << max_nodes << " there were too many nodes!"<< std::endl;
        early_exit = true;
    }

    if (final_paths.size() == 1000) {
        std::cout << "From " << from << " to " << to << " with max_size " << max_size << " and max_nodes " << max_nodes << " there were too many paths!"<< std::endl;
        early_exit = true;
    }

    if (early_exit) return {};
    return final_paths;
}

bool SequenceDistanceGraph::is_loop(std::array<sgNodeID_t, 4> nodes) {
    for (auto j = 0; j < 3; ++j)
        for (auto i = j + 1; i < 4; ++i)
            if (nodes[i] == nodes[j] or nodes[i] == -nodes[j]) return true; //looping node
    return false;
}

std::vector<sgNodeID_t> SequenceDistanceGraph::get_loopy_nodes(int complexity) {
    std::vector<sgNodeID_t> result;

#pragma omp parallel for
    for (sgNodeID_t n=1; n < nodes.size(); ++n) {
        auto fw_neigh_nodes = depth_first_search({n,0,0},0,complexity);
        auto bw_neigh_nodes = depth_first_search({-n,0,0},0,complexity);

        std::set<sgNodeID_t > fwd;
        std::set<sgNodeID_t > bwd;
        for (const auto &vn:fw_neigh_nodes) fwd.insert(std::abs(vn.node));
        for (const auto &vn:bw_neigh_nodes) bwd.insert(std::abs(vn.node));
        std::set<sgNodeID_t > tIntersect;
        std::set_intersection(fwd.begin(),fwd.end(),bwd.begin(),bwd.end(),std::inserter(tIntersect,tIntersect.begin()));
        if (!tIntersect.empty()) {
            auto fwds=get_fw_nodes(n);
            auto bwds=get_bw_nodes(n);
            std::unordered_set<sgNodeID_t> s;
            for (auto i : fwds)
                s.insert( (i>0?i:-i) );
            for (auto i : bwds)
                s.insert( (i>0?i:-i) );

            std::vector<sgNodeID_t > loop_neighbour_nodes( s.begin(), s.end() );
            std::sort( loop_neighbour_nodes.begin(), loop_neighbour_nodes.end() );
            if (loop_neighbour_nodes.size()>1) {
#pragma omp critical (loopy_nodes)
                result.push_back(n);
            }
        }
    }

    return result;
}

std::vector<sgNodeID_t> SequenceDistanceGraph::get_flanking_nodes(sgNodeID_t loopy_node) {
    auto neighs = get_neighbour_nodes(loopy_node);
    std::unordered_set<sgNodeID_t> neigh_set;
    for (const auto &n:neighs){
        auto neigh_neighs(get_neighbour_nodes(n));
        auto fwns(get_fw_nodes(n));
        auto bwns(get_bw_nodes(n));
        if (fwns == bwns and std::find(fwns.begin(), fwns.end(), loopy_node)!=fwns.end() and std::find(bwns.begin(), bwns.end(), loopy_node)!=bwns.end()) continue;
        neigh_set.insert(std::abs(n));
    }
    return std::vector<sgNodeID_t> (neigh_set.begin(), neigh_set.end());
}

void SequenceDistanceGraph::print_status() {
    auto &log_no_date=sdglib::OutputLog(sdglib::LogLevels::INFO,false);
    std::vector<uint64_t> node_sizes;
    uint64_t total_size=0;
    for (auto n:nodes) {
        if (n.status!=sgNodeDeleted) {
            total_size += n.sequence.size();
            node_sizes.push_back(n.sequence.size());
        }
    }
    std::sort(node_sizes.rbegin(),node_sizes.rend());
    auto on20s=total_size*.2;
    auto on50s=total_size*.5;
    auto on80s=total_size*.8;
    sdglib::OutputLog() <<"The graph contains "<<node_sizes.size()<<" sequences with "<<total_size<<"bp, ";
    uint64_t acc=0;
    for (auto s:node_sizes){
        if (acc==0)  log_no_date<<"N0: "<<s<<"bp  ";
        if (acc<on20s and acc+s>on20s) log_no_date<<"N20: "<<s<<"bp  ";
        if (acc<on50s and acc+s>on50s) log_no_date<<"N50: "<<s<<"bp  ";
        if (acc<on80s and acc+s>on80s) log_no_date<<"N80: "<<s<<"bp  ";
        acc+=s;
        if (acc==total_size)  log_no_date<<"N100: "<<s<<"bp  ";
    }
    log_no_date<<std::endl;
}