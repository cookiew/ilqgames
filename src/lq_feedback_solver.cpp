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
// Core LQ game solver from Basar and Olsder, "Preliminary Notation for
// Corollary 6.1" (pp. 279). All notation matches the text, though we
// shall assume that `c` (additive drift in dynamics) is always `0`, which
// holds because these dynamics are for delta x, delta us.
//
// Solve a time-varying, finite horizon LQ game (finds closed-loop Nash
// feedback strategies for both players).
//
// Assumes that dynamics are given by
//           ``` dx_{k+1} = A_k dx_k + \sum_i Bs[i]_k du[i]_k ```
//
// NOTE: Bs, Qs, ls, R1s, R2s are all lists of lists of matrices.
// NOTE: all indices of inner lists correspond to the "current time" k except
// for those of the Qs, which correspond to the "next time" k+1. That is,
// the kth entry of Qs[i] is the state cost corresponding to time step k+1. This
// makes sense because there is no point assigning any state cost to the
// initial state x_0.
//
// Returns strategies Ps, alphas.
//
///////////////////////////////////////////////////////////////////////////////

#include <ilqgames/solver/lq_feedback_solver.h>
#include <ilqgames/utils/linear_dynamics_approximation.h>
#include <ilqgames/utils/quadratic_cost_approximation.h>
#include <ilqgames/utils/strategy.h>

#include <glog/logging.h>
#include <vector>

namespace ilqgames {

std::vector<Strategy> LQFeedbackSolver::Solve(
    const std::vector<LinearDynamicsApproximation>& linearization,
    const std::vector<std::vector<QuadraticCostApproximation>>&
        quadraticization) {
  CHECK_EQ(linearization.size(), num_time_steps_);
  CHECK_EQ(quadraticization.size(), num_time_steps_);

  // List of player-indexed strategies (each of which is a time-indexed
  // affine state error-feedback controller).
  std::vector<Strategy> strategies;
  for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++)
    strategies.emplace_back(num_time_steps_, dynamics_->XDim(),
                            dynamics_->UDim(ii));

  // Initialize Zs and zetas at the final time.
  for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++) {
    Zs_[ii] = quadraticization.back()[ii].state.hess;
    zetas_[ii] = quadraticization.back()[ii].state.grad;
  }

  // Work backward in time and solve the dynamic program.
  // NOTE: time starts from the second-to-last entry since we'll treat the final
  // entry as a terminal cost as in Basar and Olsder, ch. 6.
  for (int kk = num_time_steps_ - 2; kk >= 0; kk--) {
    // Unpack linearization and quadraticization at this time step.
    const auto& lin = linearization[kk];
    const auto& quad = quadraticization[kk];

    // Populate coupling matrix S for linear matrix equation to determine X (Ps
    // and alphas).
    // NOTE: S is generally dense and asymmetric, though it is symmetric if all
    // players have the same Z.
    Dimension cumulative_udim_row = 0;
    for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++) {
      // Intermediate variable to store B[ii]' * Z[ii].
      const MatrixXf BiZi = lin.Bs[ii].transpose() * Zs_[ii];

      Dimension cumulative_udim_col = 0;
      for (PlayerIndex jj = 0; jj < dynamics_->NumPlayers(); jj++) {
        Eigen::Ref<MatrixXf> S_block =
            S_.block(cumulative_udim_row, cumulative_udim_col,
                     dynamics_->UDim(ii), dynamics_->UDim(jj));

        if (ii == jj) {
          // Does player ii's cost depend upon player jj's control?
          const auto control_iter = quad[ii].control.find(ii);
          CHECK(control_iter != quad[ii].control.end());

          S_block = BiZi * lin.Bs[ii] + control_iter->second.hess;
        } else {
          S_block = BiZi * lin.Bs[jj];
        }

        // Increment cumulative_udim_col.
        cumulative_udim_col += dynamics_->UDim(jj);
      }

      // Set appropriate blocks of Y.
      Y_.block(cumulative_udim_row, 0, dynamics_->UDim(ii), dynamics_->XDim()) =
          BiZi * lin.A;
      Y_.col(dynamics_->XDim())
          .segment(cumulative_udim_row, dynamics_->UDim(ii)) =
          lin.Bs[ii].transpose() * zetas_[ii] + quad[ii].control.at(ii).grad;

      // Increment cumulative_udim_row.
      cumulative_udim_row += dynamics_->UDim(ii);
    }

    // Solve linear matrix equality S X = Y.
    // NOTE: not 100% sure that this avoids dynamic memory allocation.
    X_ = S_.householderQr().solve(Y_);

    // Set strategy at current time step.
    for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++) {
      strategies[ii].Ps[kk] = Ps_[ii];
      strategies[ii].alphas[kk] = alphas_[ii];
    }

    // Compute F and beta.
    F_ = lin.A;
    beta_ = VectorXf::Zero(dynamics_->XDim());
    for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++) {
      F_ -= lin.Bs[ii] * Ps_[ii];
      beta_ -= lin.Bs[ii] * alphas_[ii];
    }

    // Update Zs and zetas.
    for (PlayerIndex ii = 0; ii < dynamics_->NumPlayers(); ii++) {
      zetas_[ii] = (F_.transpose() * (zetas_[ii] + Zs_[ii] * beta_) +
                    quad[ii].state.grad)
                       .eval();
      Zs_[ii] = (F_.transpose() * Zs_[ii] * F_ + quad[ii].state.hess).eval();

      // Add terms for nonzero Rijs.
      for (const auto& Rij_entry : quad[ii].control) {
        const PlayerIndex jj = Rij_entry.first;
        const MatrixXf& Rij = Rij_entry.second.hess;
        const VectorXf& rij = Rij_entry.second.grad;
        zetas_[ii] += Ps_[jj].transpose() * (Rij * alphas_[jj] - rij);
        Zs_[ii] += Ps_[jj].transpose() * Rij * Ps_[jj];
      }
    }
  }

  return strategies;
}

}  // namespace ilqgames
