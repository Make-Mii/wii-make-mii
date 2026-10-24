import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision


import sys
from collections import deque



class Segmentator():

    def __init__(self):
        frame_queue = 

        #mediapipe calculator graph init
        config_text = """
            input_stream: 'in_stream'
            output_stream: 'out_stream'
            node {
                calculator: 'PassThroughCalculator'
                input_stream: 'in_stream'
                output_stream: 'out_stream'
            }

        """


        #create graph object
        self.graph = mp.CalculatorGraph(graph_config=config_text)
        #append modified packets to this array
        self.output_packets = []

        #observer, listens to new packets being outputted and processes them
        self.graph.observe_output_stream(
            'out_stream',
            lambda stream_name, packet:
            self.output_packets.append(mp.packet_getter.get_str(packet))
        )


    self.graph.start_run()

    try:
        while (True):
            graph.add_packet_to_input_stream(

            )
    except KeyboardInterrupt:
        print("Interrupted, QUITTING")
        sys.exit(0)
