/***************************************************************************
 *            benchmarks/benchmark_binary_tree_parallel.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Tree.
 *
 *  Ariadne Tree is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Tree is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Tree. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file benchmarks/benchmark_binary_tree_parallel.cpp
 *  @brief Measure bounded parallel binary-tree recombination independently of setup costs.
 */

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
    SizeType concurrency;
    std::string method;
    Bench::Statistics statistics;
    double speedup;
    SizeType enabled_leaves;
};

// Reconfiguring the pool is part of the requested benchmark matrix,
// never part of the timed operation. Restore the CLI entry state on exit.
struct RestoreConcurrency {
    ThreadManager& manager;
    SizeType initial;
    ~RestoreConcurrency() noexcept {
        try { manager.set_concurrency(initial); }
        catch (...) { }
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

Result measure(unsigned depth,bool parallel,unsigned samples,
               BinaryTreeNode const& source,BinaryTreeNode const& oracle,
               Bench::Runner const& runner,double baseline_ns) {
    // The Benchmarking::Matrix configuration was installed before
    // entering this sample. Do not reconfigure the pool during timing.
    SizeType const concurrency=ThreadManager::instance().concurrency();
    std::string const method=parallel ? "parallel" : "serial";
    std::unique_ptr<BinaryTreeNode> current;
    auto const observations=runner.run(
        "tree/recombine/depth"+std::to_string(depth)+
        "/concurrency"+std::to_string(concurrency)+"/"+method,
        samples,
        [&] {
            current=std::make_unique<BinaryTreeNode>(source);
        },
        [&] {
            if(parallel) current->recombine_parallel();
            else current->recombine();
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
    return {depth,concurrency,method,stats,speedup,
            BinaryTreeNode::count_enabled_leaf_nodes(&source)};
}

void print_report(std::vector<Result> const& results,
                  Bench::CommandLine const& cli) {
    Bench::Report report(
        "BinaryTreeNode recombination: sequential versus internal fork/join",
        cli.summary_fields());
    report.set_parameter_header(
        "Nanoseconds per recombine; source clones, pool setup and structural"
        " correctness checks excluded from timing. Benchmarking::Matrix"
        " accepts --concurrency 0,2,4,8; each configuration compares"
        " serial and parallel recombine under the same pool setting.");
    report.set_dimension_columns({
        {"Depth",0u,Bench::TableAlignment::Right},
        {"Concurrency",0u,Bench::TableAlignment::Right},
        {"Method",0u,Bench::TableAlignment::Left},
        {"Median (ms)",0u,Bench::TableAlignment::Right},
        {"Speedup",0u,Bench::TableAlignment::Right},
        {"Efficiency",0u,Bench::TableAlignment::Right},
        {"Input leaves",0u,Bench::TableAlignment::Right}
    });
    report.set_table_options(Bench::TableOptions::ruled());
    for(auto const& row:results) {
        report.add_row({
            std::to_string(row.depth),
            std::to_string(row.concurrency),
            row.method,
            Bench::Table::number(row.statistics.median/1e6),
            Bench::Table::number(row.speedup),
            row.method=="serial" or row.concurrency<2u ? "baseline"
                : Bench::Table::number(row.speedup/static_cast<double>(row.concurrency)),
            std::to_string(row.enabled_leaves)
        },row.statistics);
    }
    report.print();
    for(auto const& row:results) {
        LOGGING_PRINTLN("TREE_PARALLEL_RECOMBINE depth="<<row.depth
            <<" concurrency="<<row.concurrency
            <<" method="<<row.method
            <<" median_ms="<<std::setprecision(9)<<row.statistics.median/1e6
            <<" speedup="<<row.speedup
            <<" efficiency="<<(row.method=="serial" or row.concurrency<2u
                ? 1.0 : row.speedup/static_cast<double>(row.concurrency)))
    }
}

} // namespace

int main(int argc,char const* argv[]) {
    try {
        unsigned samples=7u;
        ThreadManager& manager=ThreadManager::instance();
        // Benchmarking axes, not duplicate Threading runtime CLI options:
        // --concurrency accepts a scalar OR a comma-separated list.
        Bench::Parameter<unsigned> depths(
            "depths","Tree depths to measure",
            {12u,14u,16u},
            [](std::string const& text) {
                unsigned const value=Bench::parse_unsigned<unsigned>(String(text));
                if(value==0u or value>20u)
                    throw std::invalid_argument("depth must be in 1..20");
                return value;
            });
        Bench::Parameter<SizeType> concurrency(
            "concurrency","ThreadManager worker counts (0=sequential; max=hardware)",
            {0u,2u,4u,8u},
            [&manager](std::string const& text) {
                if(text=="max") return manager.maximum_concurrency();
                return Bench::parse_unsigned<SizeType>(String(text));
            });
        Bench::Matrix matrix;
        matrix.add(depths).add(concurrency);

        Bench::CommandLine cli;
        cli.add_parameters(matrix);
        cli.add(CommandLineContribution("tree-parallel-recombine",{
            make_valued_argument_parser(
                "","samples","Number of measured samples per configuration",
                [&samples](String const& text)->VoidFunction {
                    unsigned value=Bench::parse_unsigned<unsigned>(text);
                    if(value==0u)
                        throw std::invalid_argument("samples must be positive");
                    return [&samples,value] {samples=value;};
                })
        }));
        if(not cli.acquire(argc,argv)) return 0;

        RestoreConcurrency restore{manager,manager.concurrency()};
        Bench::Runner runner(cli,matrix.combinations()*2u);
        std::vector<Result> results;
        results.reserve(matrix.combinations()*2u);

        matrix.for_each([&](Bench::Configuration const& configuration) {
            unsigned const depth=configuration.value(depths);
            SizeType const workers=configuration.value(concurrency);
            if(workers>manager.maximum_concurrency()) {
                LOGGING_PRINTLN("TREE_PARALLEL_RECOMBINE_SKIPPED depth="<<depth
                    <<" concurrency="<<workers
                    <<" maximum="<<manager.maximum_concurrency())
                return;
            }

            // The pool change is required by the explicit matrix axis,
            // outside Runner's timed callbacks. Same setup for both methods.
            manager.set_concurrency(workers);
            runner.configuration_started(configuration);
            auto source=build_source(depth);
            BinaryTreeNode oracle(*source);
            oracle.recombine();

            auto baseline=measure(
                depth,false,samples,*source,oracle,runner,0.0);
            double const baseline_ns=baseline.statistics.median;
            results.push_back(std::move(baseline));
            results.push_back(measure(
                depth,true,samples,*source,oracle,runner,baseline_ns));
            runner.configuration_completed(configuration);
        });
        print_report(results,cli);
        return 0;
    } catch(std::exception const& error) {
        LOGGING_PRINTLN("benchmark_binary_tree_parallel: "<<error.what())
        return 1;
    }
}
