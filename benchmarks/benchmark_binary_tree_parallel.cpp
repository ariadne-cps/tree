/*
 * benchmark_binary_tree_parallel.cpp
 *
 * Manual benchmark of bounded internal parallel tree recombination.
 * The input copy and verification are outside Benchmarking::Runner's timer.
 */
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ariadne-benchmarking.hpp"
#include "threading/thread_manager.hpp"
#include "threading/threading_command_line_interface.hpp"
#include "tree/binary_tree.hpp"

using namespace Ariadne;
namespace Bench=Ariadne::Benchmarking;

namespace {

struct Result {
    unsigned depth;
    unsigned workers;
    Bench::Statistics statistics;
    double speedup;
    SizeType enabled_leaves;
};

struct RestoreConcurrency {
    ThreadManager& manager;
    size_t previous;
    ~RestoreConcurrency() noexcept {
        try { manager.set_concurrency(previous); } catch (...) { }
    }
};

// Create a full tree with deterministic stripes of equal leaf states.
// Adjacent uniform regions do recombine, while the resulting tree retains
// internal nodes; this stresses both reduction work and parent deletion.
void assign_states(BinaryTreeNode* node,unsigned remaining,SizeType prefix) {
    if(remaining==0u) {
        if(((prefix/4u)*37u + prefix/32u)%7u < 3u)
            node->set_disabled();
        return;
    }
    assign_states(node->left_node(),remaining-1u,prefix*2u);
    assign_states(node->right_node(),remaining-1u,prefix*2u+1u);
}

std::unique_ptr<BinaryTreeNode> build_source(unsigned depth) {
    auto result=std::make_unique<BinaryTreeNode>(true);
    result->mince(depth);
    assign_states(result.get(),depth,0u);
    return result;
}

Result measure(unsigned depth,unsigned workers,unsigned samples,
               BinaryTreeNode const& source,BinaryTreeNode const& oracle,
               Bench::Runner const& runner,double baseline_ns) {
    ThreadManager::instance().set_concurrency(workers);
    std::unique_ptr<BinaryTreeNode> current;
    auto const observations=runner.run(
        "tree/recombine/depth"+std::to_string(depth)+"/pool"+std::to_string(workers),
        samples,
        [&] {
            current=std::make_unique<BinaryTreeNode>(source);
        },
        [&] {
            if(workers==0u) current->recombine();
            else current->recombine_parallel();
            Bench::keep_alive(*current);
        },
        [&] {
            if(not current or not (*current==oracle))
                throw std::runtime_error(
                    "Internal parallel recombine differs from serial oracle");
        });
    auto const stats=Bench::summarize(observations);
    if(not std::isfinite(stats.median) or stats.median<=0.0)
        throw std::runtime_error("Invalid recombine median");
    double const speedup=baseline_ns>0.0 ? baseline_ns/stats.median : 1.0;
    return {depth,workers,stats,speedup,
            BinaryTreeNode::count_enabled_leaf_nodes(&source)};
}

void print_report(std::vector<Result> const& results,
                  Bench::CommandLine const& cli) {
    Bench::Report report(
        "BinaryTreeNode recombination: sequential versus internal fork/join",
        cli.summary_fields());
    report.set_parameter_header(
        "Nanoseconds per recombine; source clones, pool configuration and"
        " structural correctness checks excluded from timing."
        " 0=serial; other values are configured ThreadManager pool sizes.");
    report.set_dimension_columns({
        {"Depth",0u,Bench::TableAlignment::Right},
        {"Workers",0u,Bench::TableAlignment::Right},
        {"Median (ms)",0u,Bench::TableAlignment::Right},
        {"Speedup",0u,Bench::TableAlignment::Right},
        {"Efficiency",0u,Bench::TableAlignment::Right},
        {"Input leaves",0u,Bench::TableAlignment::Right}
    });
    report.set_table_options(Bench::TableOptions::ruled());
    for(auto const& row:results) {
        report.add_row({
            std::to_string(row.depth),
            std::to_string(row.workers),
            Bench::Table::number(row.statistics.median/1e6),
            Bench::Table::number(row.speedup),
            row.workers==0u ? "baseline"
                : Bench::Table::number(row.speedup/static_cast<double>(row.workers)),
            std::to_string(row.enabled_leaves)
        },row.statistics);
    }
    report.print();
    for(auto const& row:results) {
        LOGGING_PRINTLN("TREE_PARALLEL_RECOMBINE depth="<<row.depth
            <<" pool_workers="<<row.workers
            <<" median_ms="<<std::setprecision(9)<<row.statistics.median/1e6
            <<" speedup="<<row.speedup
            <<" efficiency="<<(row.workers==0u ? 1.0
                : row.speedup/static_cast<double>(row.workers)))
    }
}

} // namespace

int main(int argc,char const* argv[]) {
    try {
        unsigned samples=7u;
        std::vector<unsigned> depths{12u,14u,16u};
        std::vector<unsigned> workers{0u,2u,4u,8u};

        Bench::CommandLine cli;
        ThreadManager::instance();
        cli.add(threading_command_line_contribution());
        cli.add(CommandLineContribution("tree-parallel-recombine",{
            make_valued_argument_parser(
                "","samples","Number of measured samples per configuration",
                [&samples](String const& text)->VoidFunction {
                    unsigned value=Bench::parse_unsigned<unsigned>(text);
                    if(value==0u) throw std::invalid_argument("samples must be positive");
                    return [&samples,value] {samples=value;};
                }),
            make_valued_argument_parser(
                "","depths","Comma-separated tree depths (default: 12,14,16)",
                [&depths](String const& text)->VoidFunction {
                    auto value=Bench::parse_unique_unsigned_list(text,1u);
                    for(unsigned depth:value)
                        if(depth>20u) throw std::invalid_argument("depth exceeds 20");
                    return [&depths,value=std::move(value)] {depths=value;};
                }),
            make_valued_argument_parser(
                "","workers","Pool worker counts including 0 (default: 0,2,4,8)",
                [&workers](String const& text)->VoidFunction {
                    auto value=Bench::parse_unique_unsigned_list(text);
                    if(std::find(value.begin(),value.end(),0u)==value.end())
                        throw std::invalid_argument("workers must include serial 0");
                    return [&workers,value=std::move(value)] {workers=value;};
                })
        }));
        if(not cli.acquire(argc,argv)) return 0;

        auto& manager=ThreadManager::instance();
        RestoreConcurrency restore{manager,manager.concurrency()};
        Bench::Runner runner(cli,depths.size()*workers.size());
        std::vector<Result> results;

        for(unsigned depth:depths) {
            auto source=build_source(depth);
            BinaryTreeNode oracle(*source);
            oracle.recombine();

            // Baseline first: identical source and sampling method at each depth.
            auto baseline=measure(depth,0u,samples,*source,oracle,runner,0.0);
            double baseline_ns=baseline.statistics.median;
            results.push_back(std::move(baseline));

            for(unsigned workers_count:workers) {
                if(workers_count==0u or
                   workers_count>manager.maximum_concurrency())
                    continue;
                results.push_back(measure(
                    depth,workers_count,samples,*source,oracle,runner,baseline_ns));
            }
        }
        print_report(results,cli);
        return 0;
    } catch(std::exception const& error) {
        LOGGING_PRINTLN("benchmark_binary_tree_parallel: "<<error.what())
        return 1;
    }
}
