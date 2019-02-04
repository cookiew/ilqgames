"""
BSD 3-Clause License

Copyright (c) 2019, HJ Reachability Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Author(s): Ellis Ratner ( eratner@eecs.berkeley.edu )
"""
import rospy
from visualization_msgs.msg import Marker
from simulator import Simulator


class SimulatorNode:
    def __init__(self):
        rospy.init_node('simulator', anonymous=False)

        self.vis_pub = rospy.Publisher('visualization_marker', Marker, queue_size=10)

        config_file = rospy.get_param('~config_file', 'default.yaml')
        self.sim = Simulator(config_file)

    def run(self, freq=15.):
        rate = rospy.Rate(freq)
        dt = 1./freq
        t = 0.
        
        # TODO read this in from a message.
        inputs = {}

        while not rospy.is_shutdown():
            self.sim.step(inputs, t, dt)

            markers = self.sim.get_markers_ros()
            for m in markers: 
                self.vis_pub.publish(m)

            rate.sleep()
            t += dt


def main():
    node = SimulatorNode()

    try:
        node.run()
    except rospy.ROSInterruptException:
        pass


if __name__ == '__main__':
    main()