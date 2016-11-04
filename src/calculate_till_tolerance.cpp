#include <iostream>

//#include <tbb/task_scheduler_init.h> // <-- For controlling number of working threads
#include "tbb/parallel_for.h"

#include "bimap_cluster_populator.hpp"
#include "confusion.hpp"
#include "parallel_worker.hpp"
#include "deep_complete_simulator.hpp"
#include "calculate_till_tolerance.hpp"


//constexpr size_t  EVCOUNT_THRESHOLD = 8192;
constexpr size_t  EVCOUNT_GRAIN = 2048;
// dblp:  128 -> ;  256 -> ;  512 -> ;  1024 ->
// 50Kgt: 128 -> 2.75;  256 -> 2.63;  512 -> 2.6;  1024 -> 2.55;  >> 2048 -> 2.53; <<  4096 -> 2.8
// 50K:   128 -> 2.9;  256 -> 2.87;  512 -> 2.86;  1024 -> 2.81;  2048 -> 2.8;  > 4096 -> 2.79 <

namespace gecmi {

calculated_info_t calculate_till_tolerance(
    two_relations_ptr two_rel,
    double risk , // <-- Upper bound of probabibility of the true value being
                  //  -- farthest from estimated value than the epvar
    double epvar
    )
{
    importance_matrix_t norm_conf;
    importance_vector_t norm_cols;
    importance_vector_t norm_rows;

    // left: Nodes, right: Clusters
    size_t rows = uniqSize(two_rel->first.right) + 1;
    size_t cols = uniqSize(two_rel->second.right) + 1;

    counter_matrix_t cm =
        boost::numeric::ublas::zero_matrix< importance_float_t >( rows, cols );

    importance_float_t nmi;
    importance_float_t max_var = 1.0e10;

    deep_complete_simulator dcs( two_rel );

    // Use this to adjust number of threads
    //tbb::task_scheduler_init tsi(1);

    while( epvar < max_var )
    {

        bool raised = false;
        tbb::spin_mutex wait_for_matrix;
        try {
            // Note: simple multiplication is not enough (yields NMI 1 for small changes even on middle-size networks)
            //const size_t  steps = std::max(rows * static_cast<size_t>(cols), EVCOUNT_THRESHOLD);
            //const size_t  steps = sqrt(uniqSize(two_rel->first.left) * uniqSize(two_rel->second.left));  // The expected number of communities should not increase the square root from the number of nodes
            const size_t  steps = (rows - 1) * (cols - 1);
            parallel_for(
                tbb::blocked_range< size_t >( 0, steps, EVCOUNT_GRAIN ),  // EVCOUNT_THRESHOLD
                direct_worker< counter_matrix_t* >( dcs, &cm, &wait_for_matrix )
            );
        } catch (tbb::tbb_exception const& e) {
            std::cout << "e" << std::endl;
            raised = true;
        }

        if ( raised )
        {
            throw std::runtime_error("SystemIsSuspiciuslyFailingTooMuch ctt (maybe your partition is not solvable?)");
        }

        size_t total_events = total_events_from_unmi_cm( cm );
        normalize_events(
            cm,
            norm_conf,
            norm_cols,
            norm_rows
            );
        variances_at_prob(
            norm_conf, norm_cols, norm_rows,
            total_events,
            risk,
            max_var,
            nmi
            );
        //std::cout << total_events << " events simulated. Approx. error: " <<  max_var << std::endl;
    }

    return calculated_info_t{max_var, nmi};
}// calculate_till_tolerance

}  // gecmi
