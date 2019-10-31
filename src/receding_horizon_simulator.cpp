/*
 * Copyright (c) 2019, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Authors: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
 */

///////////////////////////////////////////////////////////////////////////////
//
// Utility for solving a problem using a receding horizon, simulating dynamics
// forward at each stage to account for the passage of time.
//
// This class is intended as a facsimile of a real-time, online receding horizon
// problem in which short horizon problems are solved asynchronously throughout
// operation.
//
///////////////////////////////////////////////////////////////////////////////

#include <ilqgames/examples/receding_horizon_simulator.h>
#include <ilqgames/solver/ilq_solver.h>
#include <ilqgames/solver/problem.h>
#include <ilqgames/solver/solution_splicer.h>
#include <ilqgames/utils/solver_log.h>
#include <ilqgames/utils/strategy.h>
#include <ilqgames/utils/types.h>

#include <glog/logging.h>
#include <chrono>
#include <memory>
#include <vector>

namespace ilqgames {

using clock = std::chrono::system_clock;

std::vector<std::shared_ptr<const SolverLog>> RecedingHorizonSimulator(
    Time final_time, Time planner_runtime, Problem* problem) {
  CHECK_NOTNULL(problem);

  // Set up a list of solver logs, one per solver invocation.
  std::vector<std::shared_ptr<const SolverLog>> logs;

  // Initial run of the solver. Keep track of time in order to know how much to
  // integrate dynamics forward.
  auto solver_call_time = clock::now();
  logs.push_back(problem->Solve());
  Time elapsed_time =
      std::chrono::duration<Time>(clock::now() - solver_call_time).count();

  VLOG(0) << "Solved initial problem in " << elapsed_time << " seconds.";

  // Handy references.
  const auto& dynamics = problem->Solver().Dynamics();
  const Time time_step = problem->Solver().TimeStep();

  // Keep a solution splicer to incorporate new receding horizon solutions.
  SolutionSplicer splicer(*logs.front());

  // Repeatedly integrate dynamics forward, reset problem initial conditions,
  // and resolve.
  VectorXf x(problem->InitialState());
  Time t = splicer.CurrentOperatingPoint().t0;
  while (t < final_time) {
    // Set up next receding horizon problem and solve.
    problem->SetUpNextRecedingHorizon(x, t, planner_runtime);

    solver_call_time = clock::now();
    logs.push_back(problem->Solve(planner_runtime));
    elapsed_time =
        std::chrono::duration<Time>(clock::now() - solver_call_time).count();

    CHECK_LE(elapsed_time, planner_runtime);
    VLOG(0) << "Solved warm-started problem in " << elapsed_time << " seconds.";

    // Integrate dynamics forward to account for solve time.
    x = dynamics.Integrate(t, t + elapsed_time, x,
                           splicer.CurrentOperatingPoint(),
                           splicer.CurrentStrategies());
    t += elapsed_time;

    // Add new solution to splicer.
    splicer.Splice(*logs.back(), t);

    // Overwrite problem with spliced solution.
    problem->OverwriteSolution(splicer.CurrentOperatingPoint(),
                               splicer.CurrentStrategies());

    // Integrate a little more.
    constexpr Time kExtraTime = 0.1;
    x = dynamics.Integrate(t, t + kExtraTime, x,
                           splicer.CurrentOperatingPoint(),
                           splicer.CurrentStrategies());
    t += kExtraTime;
  }

  return logs;
}

}  // namespace ilqgames
