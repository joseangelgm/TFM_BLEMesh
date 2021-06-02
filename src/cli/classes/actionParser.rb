require 'json'
require 'pp'
require 'io/console'
require 'string'

class ActionParser

    # attributes
    attr_reader :actions_json

    TEMP_FILE = '/tmp/mqtt.json'
    private_constant :TEMP_FILE

    OPCODES = [
        'GET_STATUS',
        'GET_CADENCE',
        'GET_DESCRIPTOR',
        'GET_SETTING',
        'GET_SERIES'
    ]
    private_constant :OPCODES

    public

    def initialize
        @actions_json = nil
    end

    def create_json(editor)
        loop do
            system editor, TEMP_FILE
            begin
                @actions_json = File.read(TEMP_FILE)
                parse_json
                if yes_no_option?
                    check_json
                    break
                end
            rescue Exception => e
                STDERR.puts "#{"Exception".bold.red} #{e.class}: #{e.message}"
                press_key_continue_prompt
            end
        end
        puts "Json is correct"
        #File.delete TEMP_FILE
    end

    private

    def parse_json
        @actions_json = JSON.parse(@actions_json)
        puts JSON.pretty_generate(@actions_json)
    end

    def check_json
        if !@actions_json.key? 'actions'
            raise KeyError, "Missing actions. Contains a list of task to be created."
        else
            @actions_json['actions'].each do |elem|
                raise KeyError, "Missing auto param. true if is a repetitive task, false otherwise." if !elem.key? 'auto'
                raise KeyError, "Missing opcode. Has to be within #{OPCODES.to_s}" if !elem.key? 'opcode'
                raise NameError, "Opcode value invalid. Has to be within #{OPCODES.to_s}" if !OPCODES.include? elem['opcode']
                raise KeyError, "Missing delay param. Contains task's delay in seconds" if !elem.key? 'delay'
                raise KeyError, "Missing name param. Contains the task's name" if !elem.key? 'name'
                puts "#{"Correct".bold.green}: #{elem}"
            end
        end
    end

    # prompt

    def yes_no_option?
        print "Is it correct? (Default=y/otherwise no): "
        option = $stdin.gets.chomp.downcase
        clean_stdin
        if option.empty? || ['y', 'yes'].include?(option)
            return true
        end
        return false
    end

    def press_key_continue_prompt
        print "Press a key to continue..."
        $stdin.getc.chomp
        clean_stdin
    end

    def clean_stdin
        $stdin.getc while $stdin.ready?
    end
end