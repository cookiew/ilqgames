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
// Static variables shared by all GUI windows.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ILQGAMES_GUI_CONTROL_SLIDERS_H
#define ILQGAMES_GUI_CONTROL_SLIDERS_H

#include <ilqgames/utils/solver_log.h>

#include <memory>
#include <vector>

namespace ilqgames {

class ControlSliders {
 public:
  ~ControlSliders() {}
  ControlSliders(
      const std::vector<std::shared_ptr<const ilqgames::SolverLog>>& logs)
      : interpolation_time_(0.0),
        solver_iterate_(0),
        log_index_(0),
        logs_(logs) {}

  // Render all the sliders in a separate window.
  void Render();

  // Accessors.
  Time InterpolationTime() const {
    return std::max(std::min(static_cast<Time>(interpolation_time_),
                             logs_[LogIndex()]->FinalTime()),
                    logs_[LogIndex()]->InitialTime());
  }
  int SolverIterate() const {
    return std::min(solver_iterate_,
                    static_cast<int>(logs_[LogIndex()]->NumIterates() - 1));
  }
  int LogIndex() const {
    return std::min(log_index_, static_cast<int>(logs_.size() - 1));
  }

 private:
  // Time at which to interpolate trajectory.
  float interpolation_time_;

  // Solver iterate to display.
  int solver_iterate_;

  // Log index to render for receding horizon problems.
  int log_index_;

  // List of all logs we might want to inspect.
  const std::vector<std::shared_ptr<const ilqgames::SolverLog>> logs_;
};  // class ControlSliders

}  // namespace ilqgames

#endif
