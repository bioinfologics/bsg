#include <iostream>
#include <fstream>
#include <sglib/processors/KmerCompressionIndex.hpp>
#include <sglib/workspace/WorkSpace.hpp>
#include "cxxopts.hpp"

enum WorkspaceFunctions{
    NONE, //This is needed because the default of a map is 0
    MAKE,
    LOG,
    DUMP,
    NODE_KCI_DUMP,
    KCI_PROFILE,
    MERGE
};

struct WorkspaceFunctionMap : public std::map<std::string, WorkspaceFunctions>
{
    WorkspaceFunctionMap()
    {
        this->operator[]("make") =  MAKE;
        this->operator[]("log") = LOG;
        this->operator[]("dump") = DUMP;
        this->operator[]("node-kci-profile") = NODE_KCI_DUMP;
        this->operator[]("kci-profile") = KCI_PROFILE;
        this->operator[]("merge") = MERGE;
    };
    ~WorkspaceFunctionMap(){}
};


void make_workspace(int argc, char** argv){
    std::vector<std::string> lr_datastores, pr_datastores, Lr_datastores;
    std::string output = "";
    std::string gfa_filename = "", kci_filename = "";
    try {
        cxxopts::Options options("bsg-workspace make", "BSG make workspace");

        options.add_options()
                ("help", "Print help")
                ("g,gfa", "input gfa file", cxxopts::value<std::string>(gfa_filename))
                ("p,paired_reads", "paired reads datastore",
                 cxxopts::value<std::vector<std::string>>(pr_datastores))
                ("l,linked_reads", "linked reads datastore",
                 cxxopts::value<std::vector<std::string>>(lr_datastores))
                ("L,long_reads", "long reads datastore",
                 cxxopts::value<std::vector<std::string>>(Lr_datastores))
                ("k,kmerspectra", "KCI kmer spectra", cxxopts::value<std::string>(kci_filename))
                ("o,output", "output file", cxxopts::value<std::string>(output));
        auto newargc = argc - 1;
        auto newargv = &argv[1];
        auto result = options.parse(newargc, newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (output == "" or gfa_filename == "") {
            throw cxxopts::OptionException(" please specify gfa and output prefix");
        }


    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }

    //===== LOAD GRAPH =====
    WorkSpace w;
    w.add_log_entry("Created with bsg-makeworkspace");
    w.getGraph().load_from_gfa(gfa_filename);
    w.add_log_entry("GFA imported from " + gfa_filename + " (" + std::to_string(w.getGraph().nodes.size() - 1) +
                    " nodes)");

    if (kci_filename != "") {
        w.getKCI().load_from_disk(kci_filename);
        w.add_log_entry("KCI kmer spectra imported from " + kci_filename);
    }

    for (auto prds:pr_datastores) {
        //create and load the datastore, and the mapper!
        w.getPairedReadDatastores().emplace_back(prds);
        w.getPairedReadMappers().emplace_back(w.getGraph(), w.getPairedReadDatastores().back(),
                                              w.uniqueKmerIndex, w.unique63merIndex);
        w.add_log_entry("PairedReadDatastore imported from " + prds + " (" +
                        std::to_string(w.getPairedReadDatastores().back().size()) + " reads)");
    }

    for (auto lrds:lr_datastores) {
        //create and load the datastore, and the mapper!
        w.getLinkedReadDatastores().emplace_back(lrds);
        w.getLinkedReadMappers().emplace_back(w.getGraph(), w.getLinkedReadDatastores().back(),
                                              w.uniqueKmerIndex, w.unique63merIndex);
        w.add_log_entry("LinkedReadDatastore imported from " + lrds + " (" +
                        std::to_string(w.getLinkedReadDatastores().back().size()) + " reads)");
    }

    for (auto Lrds:Lr_datastores) {
        //create and load the datastore, and the mapper!
        w.getLongReadDatastores().emplace_back(Lrds);
        w.getLongReadMappers().emplace_back(w.getGraph(), w.getLongReadDatastores().back());
        w.add_log_entry("LongReadDatastore imported from " + Lrds + " (" +
                        std::to_string(w.getLongReadDatastores().back().size()) + " reads)");
    }

    w.dump_to_disk(output + ".bsgws");
}

void log_workspace(int argc, char **argv){
    std::string filename;
    try {

        cxxopts::Options options("bsg-workspace log", "BSG workspace log");

        options.add_options()
                ("help", "Print help")
                ("w,workspace", "workspace filename", cxxopts::value<std::string>(filename));

        auto newargc=argc-1;
        auto newargv=&argv[1];
        auto result=options.parse(newargc,newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (result.count("workspace")==0) {
            throw cxxopts::OptionException(" please specify kmer spectra file");
        }


    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }
    WorkSpace w;
    w.load_from_disk(filename,true);
    w.print_log();
}

void dump_workspace(int argc, char **argv){
    std::string filename,gfafilename,nodeinfofilename,seqfilename;
    float minKCI=.5, maxKCI=1.25;
    size_t min_size=2000,max_size=100000000;
    try {

        cxxopts::Options options("bsg-workspace dump", "BSG workspace dump");

        options.add_options()
                ("help", "Print help")
                ("w,workspace", "workspace filename", cxxopts::value<std::string>(filename))
                ("g,gfa", "gfa output prefix", cxxopts::value<std::string>(gfafilename))
                ("n,node_info", "node info prefix",cxxopts::value<std::string>(nodeinfofilename))
                ("s,node_sequences", "selected sequences prefix", cxxopts::value<std::string>(seqfilename))
                ("min_kci", "selected sequences min KCI", cxxopts::value<float>(minKCI))
                ("max_kci", "selected sequences max KCI", cxxopts::value<float>(maxKCI))
                ("min_size", "selected sequences min size", cxxopts::value<size_t>(min_size))
                ("max_size", "selected sequences max size", cxxopts::value<size_t>(max_size));

        auto newargc=argc-1;
        auto newargv=&argv[1];
        auto result=options.parse(newargc,newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (result.count("workspace")==0) {
            throw cxxopts::OptionException(" please specify kmer spectra file");
        }


    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }
    std::cout << "Loading workspace " << std::endl;
    WorkSpace w;
    w.load_from_disk(filename);
    if (!w.getGraph().is_sane()) {
        sglib::OutputLog()<<"ERROR: sg.is_sane() = false"<<std::endl;
        //return 1;
    }
    std::cout << "Done ... " << std::endl;
    std::cout << "Dumping gfa workspace " << std::endl;
    if (not gfafilename.empty()){
        w.getGraph().write_to_gfa(gfafilename + ".gfa");
    }
    std::cout << "Done... " << std::endl;
    if (not nodeinfofilename.empty()){
        std::ofstream nif(nodeinfofilename+".csv");
        nif<<"ID, lenght, kci"<<std::endl;
        for (auto n=1;n<w.getGraph().nodes.size();++n){
            if (w.getGraph().nodes[n].status==sgNodeStatus_t::sgNodeDeleted) continue;
            nif<<n<<", "<<w.getGraph().nodes[n].sequence.size()<<", "<<w.getKCI().compute_compression_for_node(n,1)<<std::endl;
        }
    }
}

void node_kci_dump_workspace(int argc,char **argv){
    std::string filename;
    std::string node_list;
    sgNodeID_t cnode;
    std::string prefix;

    try {
        cxxopts::Options options("bsg-workspace node_kci-dump", "BSG workspace node_kci-dump");

        options.add_options()
                ("help", "Print help")
                ("w,workspace", "workspace filename", cxxopts::value<std::string>(filename))
                ("n,nodes", "node to profile", cxxopts::value<std::string>(node_list))
                ("p,prefix", "Prefix for the output file", cxxopts::value<std::string>(prefix));

        auto newargc=argc-1;
        auto newargv=&argv[1];
        auto result=options.parse(newargc,newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (result.count("workspace")==0) {
            throw cxxopts::OptionException(" please specify kmer spectra file");
        }

        //if (not seqfilename.empty()) {
        //    std::ofstream sof(seqfilename + ".fasta");
        //    for (auto n:w.select_from_all_nodes(min_size,max_size,0,UINT32_MAX, minKCI, maxKCI)){
        //        sof<<">seq"<<n<<std::endl<<w.sg.nodes[n].sequence<<std::endl;
        //    }
        //}

    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }

    WorkSpace w;
    w.load_from_disk(filename);


    // load nodes to process split the input comma separated files to a vector
    std::vector<sgNodeID_t> node_vector;
    std::istringstream iss(node_list);
    std::string cnodestr;
    while (std::getline(iss, cnodestr, ',')){

        cnode = std::stoi(cnodestr);
        std::ofstream reads_ofl("reads_profile"+std::to_string(cnode)+".cvg", std::ios_base::app);
        std::ofstream unique_ofl("unique_profile"+std::to_string(cnode)+".cvg", std::ios_base::app);
        std::ofstream assm_ofl("assmcn_profile"+std::to_string(cnode)+".cvg", std::ios_base::app);

        std::cout << "Profiling node:" << cnode << std::endl;
        std::string sequence = w.getGraph().nodes[cnode].sequence;

        std::cout << "Coverage for: " << w.getKCI().read_counts.size() << "," << w.read_counts_header.size() << std::endl;
        for (auto ri=0; ri<w.read_counts_header.size(); ++ri){

            // One extraction per set
            auto read_coverage = w.getKCI().compute_node_coverage_profile(sequence, ri);
            reads_ofl << ">Reads_"<< prefix << "_"<< cnode << "_"<< w.read_counts_header[ri] << "|";
            for (auto c: read_coverage[0]){
                reads_ofl << c << " ";
            }
            reads_ofl << std::endl;
        }

        auto coverages = w.getKCI().compute_node_coverage_profile(sequence, 0);
        unique_ofl << ">Uniqueness_"<< prefix << "_" << cnode << "|";
        for (auto c: coverages[1]){
            unique_ofl << c << " ";
        }
        unique_ofl << std::endl;

        assm_ofl << ">Graph_"<< prefix << "_"<<cnode << "|";
        for (auto c: coverages[2]){
            assm_ofl << c << " ";
        }
        assm_ofl << std::endl;

        reads_ofl.close();
        unique_ofl.close();
        assm_ofl.close();
    }
}

void kci_profile_workspace(int argc,char **argv){
    std::string filename;
    std::string prefix;
    std::string backbone_whitelist;
    try {
        cxxopts::Options options("bsg-workspace kci-profile", "BSG workspace kci-profile");

        options.add_options()
                ("help", "Print help")
                ("w,workspace", "workspace filename", cxxopts::value<std::string>(filename))
                ("p,prefix", "Prefix for the output file", cxxopts::value<std::string>(prefix));
//                    ("b,whitelist", "Backbone nodeid list", cxxopts::value<std::string>(backbone_whitelist));

        auto newargc=argc-1;
        auto newargv=&argv[1];
        auto result=options.parse(newargc,newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (result.count("workspace")==0) {
            throw cxxopts::OptionException(" please specify kmer spectra file");
        }


    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }

    WorkSpace w;
    w.load_from_disk(filename);
    std::cout << "Sacando" << std::endl;
    w.getKCI().compute_kci_profiles(prefix);
}

void merge_workspace(int argc, char **argv){
    std::vector<int> lr_datastores,pr_datastores,Lr_datastores;
    std::string output="";
    std::string base_filename="",merge_filename="";
    bool force(false);
    try {
        cxxopts::Options options("bsg-workspace merge", "BSG workspace merge");

        options.add_options()
                ("help", "Print help")
                ("w,workspace_base", "base workspace", cxxopts::value<std::string>(base_filename))
                ("m,workspace_merge", "merge workspace", cxxopts::value<std::string>(merge_filename))
                ("p,paired_reads", "Indices of paired reads datastore to copy", cxxopts::value<std::vector<int>>(pr_datastores))
                ("l,linked_reads", "Indices of linked reads datastore to copy", cxxopts::value<std::vector<int>>(lr_datastores))
                ("L,long_reads", "Indices of long reads datastore to copy", cxxopts::value<std::vector<int>>(Lr_datastores))
                ("f,force", "force copy of datastores", cxxopts::value<bool>(force))
                ("o,output", "output file", cxxopts::value<std::string>(output));
        auto newargc=argc-1;
        auto newargv=&argv[1];
        auto result=options.parse(newargc,newargv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(0);
        }

        if (output=="" or base_filename=="" or merge_filename == "") {
            throw cxxopts::OptionException(" please specify a base workspace, a merge workspace and output prefix");
        }

    } catch (const cxxopts::OptionException &e) {
        std::cout << "Error parsing options: " << e.what() << std::endl << std::endl
                  << "Use option --help to check command line arguments." << std::endl;
        exit(1);
    }

    WorkSpace base,merge,out;
    base.load_from_disk(base_filename);
    merge.load_from_disk(merge_filename);
    out.kci = base.kci;
    out.sg = base.sg;
    std::set<int> lr_filter(lr_datastores.begin(), lr_datastores.end());
    std::set<int> pr_filter(pr_datastores.begin(), pr_datastores.end());
    std::set<int> Lr_filter(Lr_datastores.begin(), Lr_datastores.end());

    std::unordered_set<std::string> base_datastores;
    for (int i = 0; i < base.paired_read_datastores.size(); ++i) {
        base_datastores.insert(base.paired_read_datastores[i].filename);
        if (pr_filter.empty() or pr_filter.count(i) == 0) {
            sglib::OutputLog()<< "Adding " << base.paired_read_datastores[i].filename << std::endl;
            out.paired_read_datastores.push_back(base.paired_read_datastores[i]);
            out.paired_read_mappers.push_back(base.paired_read_mappers[i]);
        }
    }

    for (int i = 0; i < base.linked_read_datastores.size(); ++i) {
        base_datastores.insert(base.linked_read_datastores[i].filename);
        if (lr_filter.empty() or lr_filter.count(i) == 0) {
            sglib::OutputLog()<< "Adding " << base.linked_read_datastores[i].filename << std::endl;
            out.linked_read_datastores.push_back(base.linked_read_datastores[i]);
            out.linked_read_mappers.push_back(base.linked_read_mappers[i]);
        }
    }

    for (int i = 0; i < base.long_read_datastores.size(); ++i) {
        base_datastores.insert(base.long_read_datastores[i].filename);
        if (Lr_filter.empty() or Lr_filter.count(i) == 0) {
            sglib::OutputLog()<< "Adding " << base.long_read_datastores[i].filename << std::endl;
            out.long_read_datastores.push_back(base.long_read_datastores[i]);
            out.long_read_mappers.push_back(base.long_read_mappers[i]);
        }
    }

    if (base.sg == merge.sg or force) {
        for (int i = 0; i < merge.paired_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.paired_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.paired_read_datastores[i].filename << std::endl;
                out.paired_read_datastores.push_back(merge.paired_read_datastores[i]);
                out.paired_read_mappers.push_back(merge.paired_read_mappers[i]);
            }
        }
        for (int i = 0; i < merge.linked_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.linked_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.linked_read_datastores[i].filename << std::endl;
                out.linked_read_datastores.push_back(merge.linked_read_datastores[i]);
                out.linked_read_mappers.push_back(merge.linked_read_mappers[i]);
            }
        }
        for (int i = 0; i < merge.long_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.long_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.long_read_datastores[i].filename << std::endl;
                out.long_read_datastores.push_back(merge.long_read_datastores[i]);
                out.long_read_mappers.push_back(merge.long_read_mappers[i]);
            }
        }
    } else {
        sglib::OutputLog() << "The graphs in the base and merge datastores are different, not merging mappers"
                           << std::endl;
        for (int i = 0; i < merge.paired_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.paired_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.paired_read_datastores[i].filename << " without mappings" << std::endl;
                out.paired_read_datastores.push_back(merge.paired_read_datastores[i]);
                out.paired_read_mappers.emplace_back(merge.sg,merge.paired_read_datastores[i], out.uniqueKmerIndex, out.unique63merIndex);
            }
        }
        for (int i = 0; i < merge.linked_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.linked_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.linked_read_datastores[i].filename << " without mappings" << std::endl;
                out.linked_read_datastores.push_back(merge.linked_read_datastores[i]);
                out.linked_read_mappers.emplace_back(merge.sg,merge.linked_read_datastores[i], out.uniqueKmerIndex, out.unique63merIndex);
            }
        }
        for (int i = 0; i < merge.long_read_datastores.size(); ++i) {
            if (base_datastores.find(merge.long_read_datastores[i].filename) == base_datastores.end()) {
                sglib::OutputLog()<< "Adding " << merge.long_read_datastores[i].filename << " without mappings" << std::endl;
                out.long_read_datastores.push_back(merge.long_read_datastores[i]);
                out.long_read_mappers.emplace_back(merge.sg,merge.long_read_datastores[i]);
            }
        }
    }
    out.dump_to_disk(output+".bsgws");
}

int main(int argc, char * argv[]) {
    std::cout << "bsg-workspace"<<std::endl<<std::endl;
    std::cout << "Git origin: " << GIT_ORIGIN_URL << " -> "  << GIT_BRANCH << std::endl;
    std::cout << "Git commit: " << GIT_COMMIT_HASH << std::endl<<std::endl;
    std::cout << "Executed command:"<<std::endl;

    WorkspaceFunctionMap functionMap;

    for (auto i=0;i<argc;i++) std::cout<<argv[i]<<" ";
    std::cout<<std::endl<<std::endl;
    if (argc <2){
        std::cout<<"Please specify one of the following functions: ";
        for (auto &p: functionMap) {std::cout << p.first << ", ";}
        std::cout << "to select the execution mode." << std::endl;
        exit(1);
    }

    auto function = functionMap.find(argv[1])->second;

    switch(function) {
        case MAKE:
            make_workspace(argc,argv);
            break;
        case LOG:
            log_workspace(argc,argv);
            break;
        case DUMP:
            dump_workspace(argc,argv);
            break;
        case NODE_KCI_DUMP:
            node_kci_dump_workspace(argc,argv);
            break;
        case KCI_PROFILE:
            kci_profile_workspace(argc,argv);
            break;
        case MERGE:
            merge_workspace(argc,argv);
            break;
        default:
            std::cout << "Invalid option specified." << std::endl;
            std::cout<<"Please specify one of the following functions: ";
            for (auto &p: functionMap) {std::cout << p.first << ", ";}
            std::cout << "to select the execution mode." << std::endl;
            exit(1);
            break;
    }
    exit(0);
}

