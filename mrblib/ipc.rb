module TRN
  module IPC
    class Message
      class RawData
        def initialize(key, pos, size, align, is_numeric)
          @key = key
          @pos = pos
          @size = size
          @align = align
          @is_numeric = is_numeric
        end
        attr_reader :key

        def get_pack_string
          case @size
            when 1
              return "C"
            when 2
              return "S<"
            when 4
              return "L<"
            when 8
              return "Q<"
            else
              raise "no pack string for '#{@key}' (size #{@size})"
            end
        end
        
        def inject(msg, value)
          if value.is_a? String then
            if value.length != @size then
              raise "invalid value for '#{@key}'"
            end
            msg._copy_in_raw(@pos, value)
          else
            inject(msg, [value].pack(get_pack_string))
          end
        end

        def extract(msg)
          str = msg._copy_out_raw(@pos, @size)
          if @is_numeric then
            return str.unpack(get_pack_string)[0]
          else
            return str
          end
        end
      end

      class OutPid
        def initialize(key)
          @key = key
        end
        attr_reader :key

        def extract(msg)
          msg._copy_out_pid
        end
      end

      class TransferObject
        def initialize(key, index)
          @key = key
          @index = index
        end
        attr_reader :key

        def inject(msg, value)
          msg._copy_in_object(@index, value)
        end
        
        def extract(msg)
          msg._copy_out_object(@index)
        end
      end

      class TransferCopyHandle
        def initialize(key, index)
          @key = key
          @index = index
        end
        attr_reader :key

        def inject(msg, value)
          msg._copy_in_copy_handle(@index, value)
        end
        
        def extract(msg)
          msg._copy_out_copy_handle(@index)
        end
      end

      class TransferMoveHandle
        def initialize(key, index)
          @key = key
          @index = index
        end
        attr_reader :key

        def inject(msg, value)
          msg._copy_in_move_handle(@index, value)
        end
        
        def extract(msg)
          msg._copy_out_move_handle(@index)
        end
      end

      class Buffer
        def initialize(key, type, index)
          @key = key
          @index = index
          @type = type
        end
        attr_reader :key, :index, :type

        def inject(msg, value)
          msg._set_buffer(@index, value)
        end
      end
      
      def initialize(command_id)
        @command_id = command_id
        @in_params = []
        @out_params = []
        @in_raw_size = 0
        @out_raw_size = 0
        @send_pid = false
        @recv_pid = false
        @in_object_count = 0
        @out_object_count = 0
        @in_copy_handle_count = 0
        @out_copy_handle_count = 0
        @in_move_handle_count = 0
        @out_move_handle_count = 0
        @buffers = []
        _invalidate
      end
      
      def in_object(key)
        @in_params.push(TransferObject.new(key, @in_object_count))
        @in_object_count+= 1
        _invalidate
      end

      def out_object(key)
        @out_params.push(TransferObject.new(key, @out_object_count))
        @out_object_count+= 1
        _invalidate
      end

      def in_raw(key, size, alignment)
        pos = @in_raw_size
        pos+= alignment - 1
        pos-= pos % alignment
        @in_raw_size = pos + size
        @in_params.push(RawData.new(key, pos, size, alignment, nil))
        _invalidate
      end

      def out_raw(key, size, alignment, is_numeric=false)
        pos = @out_raw_size
        pos+= alignment - 1
        pos-= pos % alignment
        @out_raw_size = pos + size
        @out_params.push(RawData.new(key, pos, size, alignment, is_numeric))
        _invalidate
      end

      def in_copy_handle(key)
        @in_params.push(TransferCopyHandle.new(key, @in_copy_handle_count))
        @in_copy_handle_count+= 1
        _invalidate
      end

      def out_copy_handle(key)
        @out_params.push(TransferCopyHandle.new(key, @out_copy_handle_count))
        @out_copy_handle_count+= 1
        _invalidate
      end

      def in_move_handle(key)
        @in_params.push(TransferMoveHandle.new(key, @in_move_handle_count))
        @in_move_handle_count+= 1
        _invalidate
      end

      def out_move_handle(key)
        @out_params.push(TransferMoveHandle.new(key, @out_move_handle_count))
        @out_move_handle_count+= 1
        _invalidate
      end

      def buffer(key, type)
        buffer = Buffer.new(key, type, @buffers.length)
        @buffers.push(buffer)
        @in_params.push(buffer)
        _invalidate
      end

      def in_pid()
        @send_pid = true
        _invalidate
      end

      def out_pid(key)
        @recv_pid = true
        @out_params.push(OutPid.new(key))
        _invalidate
      end
      
      def _pack(hash)
        @in_params.each do |prop|
          if !hash.has_key?(prop.key) then
            raise "missing '#{prop.key}'"
          end
          prop.inject(self, hash[prop.key])
        end
      end

      def _unpack()
        hash = {}
        @out_params.each do |prop|
          hash[prop.key] = prop.extract(self)
        end
        hash
      end
    end # class Message

    class MessageBuilder
      def initialize(id)
        @message = Message.new(id)
        @params = {}
        @index = 0
      end

      def in_object(value)
        @params[@index] = value
        @message.in_object(@index)
        @index+= 1
      end

      def in_raw(value, size, alignment)
        @params[@index] = value
        @message.in_raw(@index, size, alignment)
        @index+= 1
      end

      def in_u32(value)
        in_raw([value].pack("L<"), 4, 4)
      end

      def in_u64(value)
        in_raw([value].pack("Q<"), 8, 8)
      end

      def in_copy_handle(value)
        @params[@index] = value
        @message.in_copy_handle(@index)
        @index+= 1
      end

      def in_move_handle(value)
        @params[@index] = value
        @message.in_move_handle(@index)
        @index+= 1
      end

      def buffer(value, type)
        @params[@index] = value
        @message.buffer(@index, type)
        @index+= 1
      end

      def in_pid
        @message.in_pid
      end

      def out_object(key)
        @message.out_object(key)
      end

      def out_raw(key, size, alignment, is_numeric=false)
        @message.out_raw(key, size, alignment, is_numeric)
      end

      def out_u32(key)
        @message.out_raw(key, 4, 4, true)
      end

      def out_u64(key)
        @message.out_raw(key, 8, 8, true)
      end

      def out_copy_handle(key)
        @message.out_copy_handle(key)
      end

      def out_move_handle(key)
        @message.out_copy_handle(key)
      end

      def out_pid(key)
        @message.out_pid(key)
      end

      attr_reader :message
      attr_reader :params
    end
    
    class Object
      def build(id, &block)
        mb = MessageBuilder.new(id)
        mb.instance_eval &block
        return send(mb.message, mb.params)
      end
    end # class Object
  end # module IPC
end # module TRN
