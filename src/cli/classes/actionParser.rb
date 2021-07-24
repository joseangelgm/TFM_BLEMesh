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
        'GET_DESCRIPTOR',
        'GET_SETTING',
        'GET_STATUS',
        'GET_COLUMN'
    ]
    private_constant :OPCODES

    HEX_VALUES = [
        '0', '1',
        '2', '3',
        '4', '5',
        '6', '7',
        '8', '9',
        'A', 'B',
        'C', 'D',
        'E', 'F'
    ]
    private_constant :HEX_VALUES

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
        #File.delete TEMP_FILE
    end

    private

    def parse_json
        @actions_json = JSON.parse(@actions_json)
        puts JSON.pretty_generate(@actions_json)
    end

    def is_auto_required?(opcode)
        opcode == 'GET_STATUS'
    end

    def check_json
        puts "Checking if json is correct..."
        if !@actions_json.key? 'actions'
            raise Exception, "Missing actions. Contains a list of actions or task to be created/removed."
        else
            @actions_json['actions'].each do |elem|

                # remove task
                if elem.keys.length == 1
                    raise Exception, "To remove a task only add name field in json" if !elem.keys.include? 'name'
                else

                    # is opcode?
                    raise Exception, "Missing opcode. Has to be within #{OPCODES.to_s}" if !elem.key? 'opcode'
                    raise Exception, "Opcode value invalid. Has to be within #{OPCODES.to_s}" if !OPCODES.include? elem['opcode']

                    # addr
                    raise Exception, "Missing addr param. Contains the addr to send the message" if !elem.key? 'addr'
                    raise Exception, "addr is not correct. Has to be 4 length" if elem['addr'].length != 4
                    raise Exception, "addr contains an invalid character. Has to be a value within #{HEX_VALUES.to_s}" if !addr_hex_correct? elem['addr']

                    # if auto is required then delay, name
                    if is_auto_required?(elem['opcode']) && elem.key?('auto') && elem['auto']

                        # auto
                        # raise Exception, "Missing auto param. true if is a repetitive task, false otherwise." if !elem.key? 'auto'

                        #  delay
                        raise Exception, "Missing delay param. Contains task's delay in seconds" if !elem.key? 'delay'

                        # name
                        raise Exception, "Missing name param. Contains the task's name" if !elem.key? 'name'
                    else
                        # Remove, if they are, to not send unnecesary data
                        elem.delete 'auto'
                        elem.delete 'delay'
                        elem.delete 'name'
                    end

                    if elem.key? 'sensor_prop_id'
                        raise Exception, "sensor_prop_id is not correct. Has to be 4 length" if elem['sensor_prop_id'].length != 4
                        raise Exception, "sensor_prop_id contains an invalid character. Has to be a value within #{HEX_VALUES.to_s}" if !addr_hex_correct? elem['sensor_prop_id']
                    end

                end

                puts "#{"Correct".bold.green}: #{elem}"
                sleep 0.1
            end
        end
    end

    def addr_hex_correct?(addr)
        addr.each_char do |char|
            if !(HEX_VALUES.include? char.upcase)
                return false
            end
        end
        return true
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