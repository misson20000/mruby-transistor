module TRN
  module Service
    module Descriptor
      class StringDescriptor
        def initialize(string)
          @string = string
        end
        def pack
          return [2 + @string.length, 3, @string].pack("CCa128")
        end
      end
    end

    class IDsEndpoint
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
      def PostBufferAsync(size, addr)
        @object.build(0) do
          in_u32(size)
          in_u64(addr)
          out_u32(:urb_id)
        end[:urb_id]
      end
    end
    
    class IDsInterface
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
      def RegisterEndpoint(addr)
        IDsEndpoint.new(@object.build(0) do
                          in_raw([addr].pack("C"), 1, 1)
                          out_object(:endpoint)
                        end[:endpoint])
      end
      def EnableInterface
        @object.build(3) do end
      end
      def DisableInterface
        @object.build(4) do end
      end
      def AppendConfigurationData(interface_number, speed_mode, descriptor)
        @object.build(12) do
          in_raw([interface_number].pack("C"), 1, 1)
          in_u32(speed_mode)
          buffer(descriptor, 5)
        end
      end
    end
    
    class IDsService
      def initialize
        @object = TRN::get_service("usb:ds")
      end
      def close
        @object.close
      end
      def BindDevice(complex_id)
        @object.build(0) do
          in_u32(complex_id)
        end
      end
      def BindClientProcess
        @object.build(1) do
          in_copy_handle(0xffff8001)
        end
      end
      def RegisterInterface(addr)
        IDsInterface.new(@object.build(2) do
                           in_raw([addr].pack("C"), 1, 1)
                           out_object(:interface)
                         end[:interface])
      end
      def GetState()
        @object.build(4) do
          out_u32(:state)
        end[:state]
      end
      def ClearDeviceData
        @object.build(5)
      end
      
      def AddUsbStringDescriptor(string)
        @object.build(6) do
          buffer(Descriptor::StringDescriptor.new(string).pack, 5)
          out_raw(:index, 1, 1, true)
        end[:index]
      end
      def DeleteUsbStringDescriptor(index)
        @object.build(7) do
          in_raw([index].pack("C"), 1, 1)
        end
      end
      def SetUsbDeviceDescriptor(speed_mode, descriptor)
        @object.build(8) do
          in_u32(speed_mode)
          buffer(descriptor, 5)
        end
      end
      def SetBinaryObjectStore(descriptor)
        @object.build(9) do
          buffer(descriptor, 5)
        end
      end
      def Enable
        @object.build(10) do
        end
      end
      def Disable
        @object.build(11) do
        end
      end
    end
  end
end
