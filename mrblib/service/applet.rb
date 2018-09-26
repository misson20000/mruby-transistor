module TRN
  module Service
    class AppletProxy
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
      def get_session(cmd_id)
        @object.build(cmd_id) do
          out_object(:obj)
        end[:obj]
      end
      def GetCommonStateGetter
        ICommonStateGetter.new(get_session(0))
      end
      def GetSelfController
        ISelfController.new(get_session(1))
      end
      def GetWindowController
        get_session(2)
      end
      def GetAudioController
        get_session(3)
      end
      def GetDisplayController
        get_session(4)
      end
      def GetProcessWindingController
        IProcessWindingController.new(get_session(10))
      end
      def GetLibraryAppletCreator
        ILibraryAppletCreator.new(get_session(11))
      end
      def GetApplicationFunctions
        IApplicationFunctions.new(get_session(20))
      end
      def GetDebugFunctions
        IDebugFunctions.new(get_session(1000))
      end
    end
    class ICommonStateGetter
      def initialize(object)
        @object = object
      end
      def ReceiveMessage
        @object.build(1) do
          out_u32(:message)
        end[:message]
      end
    end
    class IDebugFunctions
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
      def OpenMainApplication
        IApplicationAccessor.new(
          @object.build(1) do
            out_object(:main_app)
          end[:main_app])
      end
    end
    class IApplicationAccessor
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
      def RequestForApplicationToGetForeground
        @object.build(101) do end
      end
    end
    class ILibraryAppletCreator
      def initialize(object)
        @object = object
      end
      def close
        @object.close
      end
    end
    class IApplicationProxy < AppletProxy
    end
    class ISystemAppletProxy < AppletProxy
    end
    class ILibraryAppletProxy < AppletProxy
    end
    class IOverlayAppletProxy < AppletProxy
    end
    class IApplicationProxyService
      def initialize
        @object = TRN::get_service("appletOE")
      end
      def close
        @object.close
      end
      def OpenApplicationProxy
        IApplicationProxy.new(
          @object.build(0) do
            in_u64(0)
            in_pid
            in_copy_handle(0xffff8001)
            out_object(:proxy)
          end[:proxy])
      end
    end
    class IAllSystemAppletProxiesService
      def initialize
        @object = TRN::get_service("appletAE")
      end
      def close
        @object.close
      end
      def OpenProxy(cmd_no)
        @object.build(cmd_no) do
          in_u64(0)
          in_pid
          in_copy_handle(0xffff8001)
          out_object(:proxy)
        end[:proxy]
      end
      def OpenSystemAppletProxy
        ISystemAppletProxy.new(OpenProxy(100))
      end
      def OpenLibraryAppletProxy
        ILibraryAppletProxy.new(OpenProxy(200))
      end
      def OpenOverlayAppletProxy
        IOverlayAppletProxy.new(OpenProxy(300))
      end
      def OpenSystemApplicationProxy
        IApplicationProxy.new(OpenProxy(350))
      end
      def CreateSelfLibraryAppletCreatorForDevelop
        ILibraryAppletCreator.new(
          @object.build(400) do
            in_u64
            in_pid
          out_object(:ilac)
          end[:ilac])
      end
    end
  end
end
