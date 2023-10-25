from capstone.proto import values_pb2, ground_pb2_grpc


class GroundStationServicer(ground_pb2_grpc.GroundStationServicer):
    def GetValue(self, request, context):
        print(request.tag)
        return values_pb2.Value(f64=1.8)
