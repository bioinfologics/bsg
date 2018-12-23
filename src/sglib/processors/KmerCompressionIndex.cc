//
// Created by Bernardo Clavijo (EI) on 03/12/2017.
//


#include "KmerCompressionIndex.hpp"
#include <atomic>
#include <sstream>
#include <sglib/readers/FileReader.hpp>
#include <sglib/utilities/omp_safe.hpp>

const bsgVersion_t KmerCompressionIndex::min_compat = 0x0001;

void KmerCompressionIndex::index_graph(){
    sglib::OutputLog(sglib::INFO) << "Indexing graph, Counting..."<<std::endl;
    const int k = 31;
    uint64_t total_k=0;
    for (auto &n:sg.nodes) if (n.sequence.size()>=k) total_k+=n.sequence.size()+1-k;
    graph_kmers.clear();
    graph_kmers.reserve(total_k);
    FastaRecord r;
    KmerCountFactory<FastaRecord>kcf({k});
    for (sgNodeID_t n=1;n<sg.nodes.size();++n){
        if (sg.nodes[n].sequence.size()>=k){
            r.id=n;
            r.seq=sg.nodes[n].sequence;
            kcf.setFileRecord(r);
            kcf.next_element(graph_kmers);
        }
    }
    sglib::OutputLog(sglib::INFO)<<graph_kmers.size()<<" kmers in total"<<std::endl;
    sglib::OutputLog(sglib::INFO) << "  Sorting..."<<std::endl;
    sglib::sort(graph_kmers.begin(),graph_kmers.end());
    sglib::OutputLog(sglib::INFO) << "  Merging..."<<std::endl;
    auto wi=graph_kmers.begin();
    auto ri=graph_kmers.begin();
    while (ri<graph_kmers.end()){
        if (wi.base()==ri.base()) ++ri;
        else if (*wi<*ri) {++wi; *wi=*ri;++ri;}
        else if (*wi==*ri){wi->merge(*ri);++ri;}
    }

    graph_kmers.resize(wi+1-graph_kmers.begin());
    sglib::OutputLog(sglib::INFO)<<graph_kmers.size()<<" kmers in index"<<std::endl;
    //TODO: remove kmers with more than X in count

//    std::vector<uint64_t> uniqKmer_statistics(kmerCount_SMR.summaryStatistics());
//    std::cout << "Number of " << int(k) << "-kmers seen in assembly " << uniqKmer_statistics[0] << std::endl;
//    std::cout << "Number of contigs from the assembly " << uniqKmer_statistics[2] << std::endl;
}

void KmerCompressionIndex::reindex_graph(){
    sglib::OutputLog(sglib::INFO) << "Re-indexing graph, Counting..."<<std::endl;
    std::vector<uint8_t> new_counts(graph_kmers.size());

    const int k = 31;
    std::vector<KmerCount> nodekmers;
    FastaRecord r;
    KmerCountFactory<FastaRecord>kcf({k});
    for (sgNodeID_t n=1;n<sg.nodes.size();++n){
        if (n%10000 == 0) sglib::OutputLog(sglib::INFO) << "Node id: " << n << "/" << sg.nodes.size() << std::endl;

        if (sg.nodes[n].sequence.size()>=k){
            r.id=n;
            r.seq=sg.nodes[n].sequence;
            kcf.setFileRecord(r);
            kcf.next_element(nodekmers);
            for (auto &nk:nodekmers){
                auto gk=std::lower_bound(graph_kmers.begin(),graph_kmers.end(),nk);
                if (gk->kmer==nk.kmer and new_counts[gk-graph_kmers.begin()]<255) ++new_counts[gk-graph_kmers.begin()];
            }
            nodekmers.clear();
        }
    }
    sglib::OutputLog(sglib::INFO) << "  Updating counts..."<<std::endl;
    for (auto i=0;i<graph_kmers.size();++i){
        graph_kmers[i].count=new_counts[i];
    }
    sglib::OutputLog(sglib::INFO) << "Re-indexing done."<<std::endl;
}

void KmerCompressionIndex::read(std::ifstream &input_file) {
    uint64_t kcount;
    bsgMagic_t magic;
    bsgVersion_t version;
    BSG_FILETYPE type;
    input_file.read((char *) &magic, sizeof(magic));
    input_file.read((char *) &version, sizeof(version));
    input_file.read((char *) &type, sizeof(type));

    if (magic != BSG_MAGIC) {
        throw std::runtime_error("Magic number not present in the kci file");
    }

    if (version < min_compat) {
        throw std::runtime_error("kci file version: " + std::to_string(version) + " is not compatible with " + std::to_string(min_compat));
    }

    if (type != KCI_FT) {
        throw std::runtime_error("File type supplied: " + std::to_string(type) + " is not compatible with KCI_FT");
    }

    input_file.read(( char *) &kcount,sizeof(kcount));
    graph_kmers.resize(kcount);
    input_file.read(( char *) graph_kmers.data(),sizeof(KmerCount)*kcount);
    //read-to-node
    uint64_t ccount;
    input_file.read(( char *) &ccount,sizeof(ccount));
    for (auto i=0;i<ccount;++i) {
        read_counts.emplace_back();
        read_counts.back().resize(kcount);
        input_file.read(( char *) read_counts.back().data(), sizeof(uint16_t) * kcount);
    }
    compute_compression_stats();
}

void KmerCompressionIndex::load_from_disk(std::string filename) {
    std::ifstream inf(filename);
    //read-to-tag
    read(inf);

}

void KmerCompressionIndex::write(std::ofstream &output_file) {
    uint64_t kcount=graph_kmers.size();
    output_file.write((const char *) &BSG_MAGIC, sizeof(BSG_MAGIC));
    output_file.write((const char *) &BSG_VN, sizeof(BSG_VN));
    BSG_FILETYPE type(KCI_FT);
    output_file.write((char *) &type, sizeof(type));
    output_file.write((const char *) &kcount,sizeof(kcount));
    output_file.write((const char *) graph_kmers.data(),sizeof(KmerCount)*kcount);
    uint64_t ccount=read_counts.size();
    output_file.write((const char *) &ccount,sizeof(ccount));
    for (auto i=0;i<ccount;++i) {
        output_file.write((const char *) read_counts[i].data(), sizeof(uint16_t) * kcount);
    }
}

void KmerCompressionIndex::save_to_disk(std::string filename) {
    std::ofstream of(filename);
    write(of);
}

void KmerCompressionIndex::start_new_count(){
    read_counts.emplace_back();
    read_counts.back().resize(graph_kmers.size(),0);
}

void KmerCompressionIndex::add_counts_from_file(std::vector<std::string> filenames) {


    uint64_t present(0), absent(0), rp(0);
    sglib::OutputLog(sglib::INFO)<<"Populating lookup map"<<std::endl;
    std::unordered_map<uint64_t,uint64_t> kmer_map;
    kmer_map.reserve(graph_kmers.size());
    for (uint64_t i=0;i<graph_kmers.size();++i) kmer_map[graph_kmers[i].kmer]=i;
    sglib::OutputLog(sglib::INFO)<<"Map populated, processing files"<<std::endl;
    for (auto filename:filenames) {
        sglib::OutputLog(sglib::INFO) << "Counting from file: " << filename << std::endl;
        FastqReader<FastqRecord> fastqReader({0}, filename);

#pragma omp parallel shared(fastqReader)
        {
            uint64_t thread_present(0), thread_absent(0), thread_rp(0);
            const size_t local_kmers_size = 2000000;
            std::vector<uint64_t> found_kmers;
            found_kmers.reserve(local_kmers_size);
            FastqRecord read;
            std::vector<KmerCount> readkmers;
            KmerCountFactory<FastqRecord> kf({31});

            bool c;
#pragma omp critical(fastqreader)
            {
                c = fastqReader.next_record(read);
            }
            while (c) {
                readkmers.clear();
                kf.setFileRecord(read);
                kf.next_element(readkmers);

                for (auto &rk:readkmers) {
                    auto findk = kmer_map.find(rk.kmer);
                    if (kmer_map.end() != findk) {
                        //++thread_counts[findk->second];
                        found_kmers.emplace_back(findk->second);
                        if (found_kmers.size() == local_kmers_size) {
                            bool printstatus = false;
#pragma omp critical(results_merge)
                            {
                                auto &arc = read_counts.back();
                                for (auto &x:found_kmers) if (arc[x] < UINT16_MAX) ++arc[x];
                                if (rp / 1000000 != (rp + thread_rp) / 1000000) printstatus = true;
                                rp += thread_rp;
                                present += thread_present;
                                absent += thread_absent;
                            }
                            found_kmers.clear();
                            thread_absent = 0;
                            thread_present = 0;
                            thread_rp = 0;
                            if (printstatus)
                                sglib::OutputLog(sglib::INFO) << rp << " reads processed " << present << " / "
                                                              << present + absent << " kmers found" << std::endl;
                        }
                        ++thread_present;
                    } else ++thread_absent;


                }
                ++thread_rp;
#pragma omp critical(fastqreader)
                c = fastqReader.next_record(read);
            }

#pragma omp critical(results_merge)
            {
                auto &arc = read_counts.back();
                for (auto &x:found_kmers) if (arc[x] < UINT16_MAX) ++arc[x];
                rp += thread_rp;
                present += thread_present;
                absent += thread_absent;
            }
            found_kmers.clear();

        }
        sglib::OutputLog(sglib::INFO) << rp << " reads processed " << present << " / " << present + absent
                                      << " kmers found" << std::endl;
    }
}

void KmerCompressionIndex::compute_compression_stats() {
    //compute mean, median and mode, as of now, only use the first read count
    //uint64_t graphcov[10]={0,0,0,0,0,0,0,0,0,0};
    //std::cout << "Coverage in graph:" <<std::endl;
    //for (auto &gk:graph_kmers) ++graphcov[(gk.count<10?gk.count-1:9)];
    //for (auto i=1;i<10;++i) std::cout << i <<":   "<<graphcov[i-1]<<std::endl;
    //std::cout <<"10+: "<<graphcov[9]<<std::endl;
    uint64_t covuniq[1001];
    for (auto &c:covuniq)c=0;
    uint64_t tuniq=0,cuniq=0;
    for (uint64_t i=0; i<graph_kmers.size(); ++i){
        if (graph_kmers[i].count==1){
            tuniq+=read_counts[0][i];
            ++cuniq;
            ++covuniq[(read_counts[0][i]<1000 ? read_counts[0][i] : 1000 )];
        }
    }
    uint64_t cseen=0,median=0;
    while (cseen<cuniq/2) {cseen+=covuniq[median];++median;};
    uint64_t mode=0;
    for (auto i=0;i<1000;++i) if (covuniq[i]>covuniq[mode]) mode=i;
    sglib::OutputLog()<<"KCI Mean coverage for unique kmers:   " << ((double)tuniq)/cuniq <<std::endl;
    sglib::OutputLog()<<"KCI Median coverage for unique kmers: " << median <<std::endl;
    sglib::OutputLog()<<"KCI Mode coverage for unique kmers:   " << mode <<std::endl;

    if ( std::abs( float(median-mode) ) > mode*.1) sglib::OutputLog()<<"WARNING -> median and mode highly divergent"<<std::endl;
    uniq_mode=mode;

}

void KmerCompressionIndex::dump_histogram(std::string filename) {
    std::ofstream kchf(filename);
    uint64_t covuniq[1001];
    for (auto &c:covuniq)c=0;
    uint64_t tuniq=0,cuniq=0;
    for (uint64_t i=0; i<graph_kmers.size(); ++i){
        if (graph_kmers[i].count==1){
            tuniq+=read_counts[0][i];
            ++cuniq;
            ++covuniq[(read_counts[0][i]<1000 ? read_counts[0][i] : 1000 )];
        }
    }
    for (auto i=0;i<1000;++i) kchf<<i<<","<<covuniq[i]<<std::endl;
}

void KmerCompressionIndex::dump_comp_mx(std::string filename, int kmer_collection){
    std::ofstream okm(filename);
    std::cout << "File open..." << std::endl;
    std::cout << "Graph kmers: "<< this->graph_kmers.size() << std::endl;

    int max_read_cvg = 1001;
    int max_graph_cvg = 1001;
    std::vector<std::vector<int>> matrix(max_read_cvg, std::vector<int>(max_read_cvg));

    // Count values and accumulate in the matrix
    std::cout << "Counting values " << std::endl;
    for (uint64_t i=0; i<this->graph_kmers.size(); ++i){
        auto read_cvg = this->read_counts[kmer_collection][i];
        auto graph_cvg = graph_kmers[i].count;
        if (read_cvg < max_read_cvg & graph_cvg < max_graph_cvg){
            matrix[read_cvg][graph_cvg]++;
        }
    }

    // Write the headers
    std::cout << "Dumping matrix " << std::endl;
    okm << "# Title:K-mer comparison plot generated with BSG" << std::endl;
    okm << "# XLabel:27-mer frequency for: kmer read collection" << std::endl;
    okm << "# YLabel:27-mer frequency for: graph kmers" << std::endl;
    okm << "# ZLabel:# distinct 31-mers" << std::endl;
    okm << "# Columns:1001" << std::endl;
    okm << "# Rows:1001" << std::endl;
    okm << "# MaxVal:NA" << std::endl;
    okm << "# Transpose:1" << std::endl;
    okm << "# Kmer value:NA" << std::endl;
    okm << "# Input 1: Kmer read collection" << std::endl;
    okm << "# Input 2: Graph" << std::endl;
    okm << "###" << std::endl;

    // Dump matrix
    for(auto &row: matrix){
        for (auto &column: row){
            okm << column << " ";
        }
        okm << std::endl;
    }
}

double KmerCompressionIndex::compute_compression_for_node(sgNodeID_t _node, uint16_t max_graph_freq, uint16_t dataset) {
    const int k=31;
    auto n=_node>0 ? _node:-_node;
    auto & node=sg.nodes[n];

    //eliminate "overlapping" kmers
    int32_t max_bw_ovlp=0;
    int32_t max_fw_ovlp=0;
    for (auto bl:sg.get_bw_links(n)) {
        if (bl.dist<0){
            auto ovl=-bl.dist+1-k;
            if (ovl>max_bw_ovlp) max_bw_ovlp=ovl;
        }
    }
    for (auto fl:sg.get_fw_links(n)) {
        if (fl.dist<0){
            auto ovl=-fl.dist+1-k;
            if (ovl>max_fw_ovlp) max_fw_ovlp=ovl;
        }
    }
    int64_t newsize=node.sequence.size();
    newsize=newsize-max_bw_ovlp-max_fw_ovlp;
    //if (n/10==50400){
    //    std::cout<<"node "<<n<<" size="<<node.sequence.size()<<" max_bw_olv="<<max_bw_ovlp<<" max_fw_ovl="<<max_fw_ovlp<<" newlength="<<newsize<<std::endl;
    //}
    if (newsize<k) return ((double)0/0);
    auto s=node.sequence.substr(max_bw_ovlp,newsize);
    std::vector<uint64_t> nkmers;
    StringKMerFactory skf(k);


    skf.create_kmers(s,nkmers);

    uint64_t kcount=0,kcov=0;
    for (auto &kmer : nkmers){
        auto nk = std::lower_bound(graph_kmers.begin(), graph_kmers.end(), KmerCount(kmer,0));
        if (nk!=graph_kmers.end() and nk->kmer == kmer and nk->count<=max_graph_freq) {
            kcount+=nk->count;
            kcov+=read_counts[dataset][nk-graph_kmers.begin()];
        }
    }

    return (((double) kcov)/kcount )/uniq_mode;
}

void KmerCompressionIndex::add_counts_from_datastore(const PairedReadsDatastore &ds) {
    uint64_t present(0), absent(0), rp(0);
    sglib::OutputLog(sglib::INFO)<<"Populating lookup map"<<std::endl;
    std::unordered_map<uint64_t,uint64_t> kmer_map;
    kmer_map.reserve(graph_kmers.size());
    for (uint64_t i=0;i<graph_kmers.size();++i) kmer_map[graph_kmers[i].kmer]=i;
    sglib::OutputLog(sglib::INFO)<<"Map populated, counting from datastore: " << ds.filename << std::endl;

#pragma omp parallel
    {
        BufferedPairedSequenceGetter bpsg(ds,100000,1000);
        uint64_t thread_present(0), thread_absent(0), thread_rp(0);
        const size_t local_kmers_size = 2000000;
        std::vector<uint64_t> found_kmers;
        found_kmers.reserve(local_kmers_size);
        std::vector<KmerCount> readkmers;
        CStringKMerFactory cskf(31);
        auto &arc = read_counts.back();
#pragma omp for schedule(static,10000)
        for (uint64_t rid = 1; rid <= ds.size(); ++rid) {
            readkmers.clear();
            cskf.create_kmercounts(readkmers, bpsg.get_read_sequence(rid));

            for (auto &rk:readkmers) {
                auto findk = kmer_map.find(rk.kmer);
                if (kmer_map.end() != findk) {
                    //++thread_counts[findk->second];
                    found_kmers.emplace_back(findk->second);
                    if (found_kmers.size() == local_kmers_size) {
                        bool printstatus = false;
#pragma omp critical(results_merge)
                        {

                            for (auto &x:found_kmers) if (arc[x] < UINT16_MAX) ++arc[x];
                            if (rp / 1000000 != (rp + thread_rp) / 1000000) printstatus = true;
                            rp += thread_rp;
                            present += thread_present;
                            absent += thread_absent;
                        }
                        found_kmers.clear();
                        thread_absent = 0;
                        thread_present = 0;
                        thread_rp = 0;
                        if (printstatus)
                            sglib::OutputLog(sglib::INFO) << rp << " reads processed " << present << " / "
                                                          << present + absent << " kmers found" << std::endl;
                    }
                    ++thread_present;
                } else ++thread_absent;


            }
            ++thread_rp;
        }
    }
    sglib::OutputLog(sglib::INFO) << rp << " reads processed " << present << " / " << present + absent
                                  << " kmers found" << std::endl;
}

void KmerCompressionIndex::compute_all_nodes_kci(uint16_t max_graph_freq) {
    nodes_depth.resize(sg.nodes.size());
#pragma omp parallel for shared(nodes_depth) schedule(static, 100)
    for (auto n=1;n<sg.nodes.size();++n) {
        nodes_depth[n]=compute_compression_for_node(n, max_graph_freq);
    }
}

std::vector<std::vector<uint16_t>>
KmerCompressionIndex::compute_node_coverage_profile(std::string node_sequence, int read_set_index) {
    // takes a node and returns the kci vector for the node
    const int k=31;
//    std::cout << "Number of kmers in sequence: " << node_sequence.size()-k+1 << std::endl;

    std::vector<uint64_t> nkmers;
    StringKMerFactory skf(k);
    skf.create_kmers(node_sequence,nkmers);
    std::vector<uint16_t> reads_kmer_profile;
    std::vector<uint16_t> unique_kmer_profile;
    std::vector<uint16_t> graph_kmer_profile;

    uint16_t max_graph_freq = 1;
    for (auto &kmer: nkmers){
        auto nk = std::lower_bound(graph_kmers.begin(), graph_kmers.end(), KmerCount(kmer,0));

        if (nk!=graph_kmers.end() and nk->kmer == kmer) {
            reads_kmer_profile.push_back(read_counts[read_set_index][nk-graph_kmers.begin()]);

        } else {
            reads_kmer_profile.push_back(0);
        }
        if (nk!=graph_kmers.end() and nk->kmer == kmer and nk->count <= max_graph_freq){
            unique_kmer_profile.push_back(nk->count);
        } else {
            unique_kmer_profile.push_back(0);
        }
        if (nk!=graph_kmers.end() and nk->kmer == kmer){
            graph_kmer_profile.push_back(nk->count);
        } else {
            graph_kmer_profile.push_back(0);
        }
    }
//    std::cout << "Tamanio" << reads_kmer_profile.size() <<std::endl;
    return {reads_kmer_profile, unique_kmer_profile, graph_kmer_profile};
}

void KmerCompressionIndex::compute_kci_profiles(std::string filename) {
    // vector to store vector of zero counts
    std::ofstream of(filename+"_kci.csv");


//    for (sgNodeID_t n: node_whitelist){
        // if para chequear que el nodo esta en el grafo
#pragma omp parallel for schedule(static, 20)
    for (auto n=1; n<sg.nodes.size(); ++n){
        if (sg.nodes[n].status == sgNodeDeleted) continue;
        std::stringstream ss;
        ss << "seq" << n <<" | ";
        // TODO: complete this to throw a warning when accessing a deleted node
        for (auto var=0; var < read_counts.size(); ++var) {
            auto zero_count = 0;
            auto read_coverage = compute_node_coverage_profile(sg.nodes[n].sequence, var);
            for (auto c: read_coverage[0]) {
                // TODO: Corregir este threhold
                if (c < 3){
                    zero_count++;
                }
            }
            ss << zero_count << ",";
        }
#pragma omp critical
        of << ss.str() << std::endl;
    }
}